#include <coroutine>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>
#include <unordered_set>

#include "../Protocols/MySQL/BinlogProcessor.hh"
#include "../Protocols/MySQL/Client.hh"

using namespace std;
using namespace EventAsync::MySQL;

EventAsync::DetachedTask read_binlogs(
    EventAsync::Base& base,
    const char* host,
    uint16_t port,
    const char* username,
    const char* password,
    const char* start_filename = nullptr,
    uint64_t start_position = 0) {
  EventAsync::MySQL::Client client(base, host, port, username, password);
  co_await client.connect();

  string filename = start_filename ? start_filename : "";
  if (filename.empty() || start_position == 0) {
    fprintf(stderr, "Reading master position from server\n");
    auto result = co_await client.query("SHOW MASTER STATUS");
    const auto& rows = result.rows_dicts();
    if (rows.size() != 1) {
      throw runtime_error("SHOW MASTER STATUS did not return one row");
    }
    const auto& row = rows[0];
    if (filename.empty()) {
      filename = get<string>(row.at("File"));
    }
    if (start_position == 0) {
      start_position = get<uint64_t>(row.at("Position"));
    }
  }

  co_await client.read_binlogs(filename, start_position);
  fprintf(stdout, "-- starting at %s:%" PRIu64 "\n\n", filename.c_str(), start_position);

  auto print_pos_comment_start = +[](const BinlogEventHeader& header, const string& filename) {
    uint32_t start_offset = header.end_position - header.length;
    string time_str = format_time(
        static_cast<uint64_t>(header.timestamp) * 1000000);
    if (header.timestamp == 0 && header.end_position == 0) {
      fprintf(stdout, "/* (artificial event) server %u ", header.server_id);
    } else {
      fprintf(stdout, "/* %s:%u @ %s server %u ",
          filename.c_str(), start_offset, time_str.c_str(), header.server_id);
    }
  };

  BinlogProcessor proc;
  for (;;) {
    string data = co_await client.get_binlog_event();

    const auto* header = proc.get_event_header(data);
    switch (header->type) {
      // We don't print anything for these event types
      case EventAsync::MySQL::BinlogEventType::PREVIOUS_GTIDS_EVENT:
      case EventAsync::MySQL::BinlogEventType::ANONYMOUS_GTID_EVENT:
        proc.parse_unknown_event(data);
        continue;

      case EventAsync::MySQL::BinlogEventType::TABLE_MAP_EVENT:
        proc.parse_table_map_event(data);
        break;

      case EventAsync::MySQL::BinlogEventType::WRITE_ROWS_EVENTv0:
      case EventAsync::MySQL::BinlogEventType::UPDATE_ROWS_EVENTv0:
      case EventAsync::MySQL::BinlogEventType::DELETE_ROWS_EVENTv0:
      case EventAsync::MySQL::BinlogEventType::WRITE_ROWS_EVENTv1:
      case EventAsync::MySQL::BinlogEventType::UPDATE_ROWS_EVENTv1:
      case EventAsync::MySQL::BinlogEventType::DELETE_ROWS_EVENTv1:
      case EventAsync::MySQL::BinlogEventType::WRITE_ROWS_EVENTv2:
      case EventAsync::MySQL::BinlogEventType::UPDATE_ROWS_EVENTv2:
      case EventAsync::MySQL::BinlogEventType::DELETE_ROWS_EVENTv2: {
        auto ev = proc.parse_rows_event(data);

        for (const auto& row : ev.rows) {
          print_pos_comment_start(ev.header, filename);

          if (ev.write_type == BinlogRowsEvent::WriteType::INSERT) {
            fprintf(stdout, "*/ INSERT INTO `%s`.`%s` VALUES (",
                ev.ti->database_name.c_str(), ev.ti->table_name.c_str());
            for (size_t x = 0; x < row.post.size(); x++) {
              string s = format_cell_value(row.post[x]);
              fprintf(stdout, "%s%s", ((x > 0) ? ", " : ""), s.c_str());
            }
            fputc(')', stdout);

          } else if (ev.write_type == BinlogRowsEvent::WriteType::UPDATE) {
            fprintf(stdout, "*/ UPDATE `%s`.`%s` SET ",
                ev.ti->database_name.c_str(), ev.ti->table_name.c_str());
            for (size_t x = 0; x < row.post.size(); x++) {
              string s = format_cell_value(row.post[x]);
              fprintf(stdout, "%s#%zu = %s", ((x > 0) ? ", " : ""), x, s.c_str());
            }

            if (!row.pre.empty()) {
              fputs(" WHERE ", stdout);
              for (size_t x = 0; x < row.pre.size(); x++) {
                string s = format_cell_value(row.pre[x]);
                fprintf(stdout, "%s#%zu = %s", ((x > 0) ? " AND " : ""), x, s.c_str());
              }
            } else {
              fputs(" /* MISSING PRE-IMAGE */", stdout);
            }

          } else if (ev.write_type == BinlogRowsEvent::WriteType::DELETE) {
            fprintf(stdout, "*/ DELETE FROM `%s`.`%s` WHERE ",
                ev.ti->database_name.c_str(), ev.ti->table_name.c_str());
            for (size_t x = 0; x < row.pre.size(); x++) {
              string s = format_cell_value(row.pre[x]);
              fprintf(stdout, "%s#%zu = %s", ((x > 0) ? " AND " : ""), x, s.c_str());
            }

          } else {
            throw logic_error("row event is not insert, update, or delete");
          }
          fputs(";\n", stdout);
        }
        break;
      }

      case EventAsync::MySQL::BinlogEventType::QUERY_EVENT: {
        auto ev = proc.parse_query_event(data);
        if (ev.query != "BEGIN" && ev.query != "COMMIT") {
          print_pos_comment_start(ev.header, filename);
          if (ev.database_name.empty()) {
            fprintf(stdout, "conn %u (%us) */ %s;\n", ev.conn_id,
                ev.exec_time_secs, ev.query.c_str());
          } else {
            fprintf(stdout, "conn %u db %s (%us) */ %s;\n", ev.conn_id,
                ev.database_name.c_str(), ev.exec_time_secs, ev.query.c_str());
          }
        }
        break;
      }

      case EventAsync::MySQL::BinlogEventType::STOP_EVENT: {
        auto header = proc.parse_unknown_event(data);
        print_pos_comment_start(header, filename);
        fprintf(stdout, "*/ SHUTDOWN;\n");
        break;
      }

      case EventAsync::MySQL::BinlogEventType::XID_EVENT: {
        proc.parse_xid_event(data);
        // print_pos_comment_start(header, filename);
        // fprintf(stdout, "xid %" PRIu64 " */ COMMIT;\n", r.get_u64());
        break;
      }

      case EventAsync::MySQL::BinlogEventType::ROTATE_EVENT: {
        auto ev = proc.parse_rotate_event(data);
        print_pos_comment_start(ev.header, filename);
        fprintf(stdout, "next log pos %s:%" PRIu64 " */\n",
            ev.next_filename.c_str(), ev.next_position);
        filename = ev.next_filename;
        break;
      }

      case EventAsync::MySQL::BinlogEventType::FORMAT_DESCRIPTION_EVENT: {
        proc.parse_format_description_event(data);
        break;
      }

      default: {
        auto header = proc.parse_unknown_event(data);
        print_pos_comment_start(header, filename);
        fprintf(stdout, "unknown event %s */\n",
            name_for_binlog_event_type(header.type));
        print_data(stdout, data);
      }
    }
  }
}

int main(int argc, char** argv) {
  if (argc < 5 || argc > 7) {
    throw invalid_argument("Usage: BinlogReader hostname port username password [filename start_position]\n");
  }

  const char* host = argv[1];
  uint16_t port = atoi(argv[2]);
  const char* username = argv[3];
  const char* password = argv[4];
  const char* filename = argc > 5 ? argv[5] : nullptr;
  uint64_t start_position = argc > 6 ? strtoull(argv[6], nullptr, 0) : 0;

  EventAsync::Base base;
  read_binlogs(base, host, port, username, password, filename, start_position);
  base.run();
  return 0;
}
