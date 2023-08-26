#include <string.h>

#include <coroutine>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>
#include <phosg/Time.hh>
#include <unordered_set>

#include "../Protocols/MySQL/BinlogProcessor.hh"
#include "../Protocols/MySQL/Client.hh"

using namespace std;
using namespace EventAsync::MySQL;

class StatsDClient {
public:
  StatsDClient(
      EventAsync::Base& base,
      const string& hostname = "",
      uint16_t port = 8125)
      : hostname(hostname),
        port(port),
        fd(-1),
        writing_stdout(false),
        buf(base) {
    if (this->hostname.empty()) {
      fprintf(stderr, "sending output to stdout\n");
      this->fd = fileno(stdout);
      this->writing_stdout = true;

    } else {
      fprintf(stderr, "sending output to %s:%hu\n", this->hostname.c_str(), this->port);
      pair<struct sockaddr_storage, size_t> s = make_sockaddr_storage(
          this->hostname, this->port);

      this->fd = socket(s.first.ss_family, SOCK_DGRAM, IPPROTO_UDP);
      if (this->fd == -1) {
        throw runtime_error("can\'t create socket: " + string_for_error(errno));
      }

      make_fd_nonblocking(fd);

      int connect_ret = connect(this->fd, (struct sockaddr*)&s.first, s.second);
      if ((connect_ret == -1) && (errno != EINPROGRESS)) {
        close(this->fd);
        this->fd = -1;
        throw runtime_error("can\'t connect socket: " + string_for_error(errno));
      }
    }
  }

  ~StatsDClient() {
    if (!this->writing_stdout && (this->fd >= 0)) {
      close(this->fd);
    }
  }

  EventAsync::Task<void> send(
      const string& metric,
      double value,
      char type, // [cgmhsd]; 'm' is special-cased and expands to 'ms'
      const unordered_map<string, string>& tags = {},
      double sample_rate = 1.0) {
    if (this->buf.get_length()) {
      throw logic_error("Buffer was not empty before statsd line generation");
    }

    this->buf.add_reference(metric);
    this->buf.add_printf(":%lg|%c", value, type);
    if (type == 'm') {
      this->buf.add("s", 1);
    }
    if (sample_rate != 1.0) {
      this->buf.add_printf("|@%lg", sample_rate);
    }
    if (!tags.empty()) {
      bool is_first = true;
      for (const auto& tag : tags) {
        if (is_first) {
          this->buf.add("|#", 2);
        } else {
          this->buf.add(",", 1);
        }
        if (tag.second.empty()) {
          this->buf.add(tag.first.c_str());
        } else {
          this->buf.add_printf("%s:%s", tag.first.c_str(), tag.second.c_str());
        }
      }
    }

    if (this->writing_stdout) {
      this->buf.add("\n", 1);
    }

    co_await this->buf.write(this->fd);
  }

private:
  string hostname;
  uint16_t port;
  int fd;
  bool writing_stdout;
  EventAsync::Buffer buf;
};

struct Options {
  const char* host;
  uint16_t port;
  const char* username;
  const char* password;
  const char* start_filename;
  uint64_t start_position;

  const char* stats_host;
  uint16_t stats_port;
  unordered_map<string, string> constant_tags;

  Options()
      : host("127.0.0.1"),
        port(3306),
        username("root"),
        password("root"),
        start_filename(nullptr),
        start_position(0),
        stats_host(""),
        stats_port(8125) {}
};

EventAsync::DetachedTask generate_binlog_stats(
    EventAsync::Base& base, const Options& opts) {

  EventAsync::MySQL::Client client(
      base, opts.host, opts.port, opts.username, opts.password);
  co_await client.connect();

  string current_filename = opts.start_filename;
  uint64_t current_position = opts.start_position;
  if (current_filename.empty() || (current_position == 0)) {
    fprintf(stderr, "reading master position from server\n");
    auto result = co_await client.query("SHOW MASTER STATUS");
    const auto& rows = result.rows_dicts();
    if (rows.size() != 1) {
      throw runtime_error("SHOW MASTER STATUS did not return one row");
    }
    const auto& row = rows[0];
    if (current_filename.empty()) {
      current_filename = get<string>(row.at("File"));
    }
    if (current_position == 0) {
      current_position = get<uint64_t>(row.at("Position"));
    }
  }

  StatsDClient statsd(base, opts.stats_host, opts.stats_port);

  co_await client.read_binlogs(current_filename, current_position);
  fprintf(stderr, "starting at %s:%" PRIu64 "\n\n",
      current_filename.c_str(), current_position);

  BinlogProcessor proc;
  size_t transaction_event_bytes = 0;
  for (;;) {
    string data = co_await client.get_binlog_event();
    transaction_event_bytes += data.size();

    const BinlogEventHeader* header = proc.get_event_header(data);
    // Artificial events have 0 in this field. We'll calculate an incorrect
    // current_position in that case, so just ignore them. The -4 corrects for
    // the checksum, which is stripped off by get_binlog_event.
    if (header->end_position) {
      current_position = header->end_position - data.size() - 4;
    }

    auto tags = opts.constant_tags;
    tags.emplace("binlog_event_type", name_for_binlog_event_type(header->type));
    co_await statsd.send("mysql.binlog.event_bytes", data.size(), 'd', tags);
    tags.erase("binlog_event_type");

    // Only send current_position if it's set properly. (If the first event is
    // an artifical event, which it always is, this won't be set yet.)
    if (current_position) {
      co_await statsd.send("mysql.binlog.parser_position", current_position, 'g', tags);
    }

    switch (header->type) {
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

        tags.emplace("db_name", ev.ti->database_name);
        tags.emplace("table_name", ev.ti->table_name);

        size_t pre_bytes = 0, post_bytes = 0;
        for (const auto& row : ev.rows) {
          pre_bytes += row.pre_bytes;
          post_bytes += row.post_bytes;
        }

        switch (ev.write_type) {
          case BinlogRowsEvent::WriteType::INSERT:
            co_await statsd.send("mysql.binlog.insert_events", 1, 'c', tags);
            co_await statsd.send("mysql.binlog.insert_rows", ev.rows.size(), 'd', tags);
            co_await statsd.send("mysql.binlog.insert_bytes", post_bytes, 'd', tags);
            break;
          case BinlogRowsEvent::WriteType::UPDATE:
            co_await statsd.send("mysql.binlog.update_events", 1, 'c', tags);
            co_await statsd.send("mysql.binlog.update_rows", ev.rows.size(), 'd', tags);
            co_await statsd.send("mysql.binlog.update_pre_bytes", pre_bytes, 'd', tags);
            co_await statsd.send("mysql.binlog.update_post_bytes", post_bytes, 'd', tags);
            co_await statsd.send("mysql.binlog.update_delta_bytes", post_bytes - pre_bytes, 'd', tags);
            break;
          case BinlogRowsEvent::WriteType::DELETE:
            co_await statsd.send("mysql.binlog.delete_events", 1, 'c', tags);
            co_await statsd.send("mysql.binlog.delete_rows", ev.rows.size(), 'd', tags);
            co_await statsd.send("mysql.binlog.delete_bytes", pre_bytes, 'd', tags);
            break;
          default:
            throw logic_error("invalid write_type in rows event");
        }

        break;
      }

      case EventAsync::MySQL::BinlogEventType::QUERY_EVENT: {
        auto ev = proc.parse_query_event(data);
        if (!ev.database_name.empty()) {
          tags.emplace("db_name", ev.database_name);
        }

        if (ev.query == "BEGIN") {
          transaction_event_bytes = data.size();
          co_await statsd.send("mysql.binlog.transaction_begin", 1, 'c', tags);
        } else if (ev.query == "COMMIT") {
          co_await statsd.send("mysql.binlog.transaction_commit", 1, 'c', tags);
          co_await statsd.send("mysql.binlog.transaction_event_bytes", transaction_event_bytes, 'd', tags);
          transaction_event_bytes = 0;
        } else {
          co_await statsd.send("mysql.binlog.query", 1, 'c', tags);
        }
        break;
      }

      case EventAsync::MySQL::BinlogEventType::STOP_EVENT: {
        proc.parse_unknown_event(data);
        break;
      }

      case EventAsync::MySQL::BinlogEventType::XID_EVENT: {
        proc.parse_xid_event(data);
        co_await statsd.send("mysql.binlog.transaction_commit_xid", 1, 'c', tags);
        co_await statsd.send("mysql.binlog.transaction_event_bytes", transaction_event_bytes, 'd', tags);
        transaction_event_bytes = 0;
        break;
      }

      case EventAsync::MySQL::BinlogEventType::ROTATE_EVENT: {
        auto ev = proc.parse_rotate_event(data);
        current_filename = ev.next_filename;

        // Get the numeric component of the binlog filename, ignoring
        // non-numeric chars. In most setups the binlog filename prefix is
        // entirely non-numeric, so this number should uniquely identify the
        // suffix of the binlog file, which is useful when reading binlogs for
        // manual investigations.
        uint64_t filename_number = 0;
        for (char ch : current_filename) {
          if (isdigit(ch)) {
            filename_number = (filename_number * 10) + (ch - '0');
          }
        }

        co_await statsd.send("mysql.binlog.rotate_binlog_file", 1, 'c', tags);
        co_await statsd.send("mysql.binlog.parser_file_index", filename_number, 'g', tags);
        break;
      }

      case EventAsync::MySQL::BinlogEventType::FORMAT_DESCRIPTION_EVENT:
        proc.parse_format_description_event(data);
        break;

      case EventAsync::MySQL::BinlogEventType::PREVIOUS_GTIDS_EVENT:
      case EventAsync::MySQL::BinlogEventType::ANONYMOUS_GTID_EVENT:
        proc.parse_unknown_event(data);
        break;

      default: {
        proc.parse_unknown_event(data);
        co_await statsd.send("mysql.binlog.unknown_event", 1, 'c', tags);
      }
    }
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "\
Usage: BinlogStats [options]\n\
\n\
Options:\n\
  --host=HOST, --port=PORT: Connect to this MySQL server.\n\
  --username=USER, --password=PWD: Authenticate with these credentials.\n\
  --password-env=VARNAME: Get the password from this environment variable\n\
      instead. (This is more secure than --password.)\n\
  --filename=FILENAME: Start reading from this binlog filename on the server.\n\
  --position=POSITION: Start reading from this binlog file offset on the\n\
      server. Undefined behavior may result if this position isn't the start of\n\
      a valid binlog event.\n\
  --stats-host=HOST, --stats-port=PORT: Send generated metrics here. If these\n\
      are not given, metrics are written to stdout instead.\n\
  --tag=VALUE: Send this tag along with all generated metrics.\n\
  --tag=KEY=VALUE: Send this tag along with all generated metrics.\n\
\n");
    throw invalid_argument("no arguments given");
  }

  Options opts;
  for (int x = 1; x < argc; x++) {
    if (!strncmp(argv[x], "--host=", 7)) {
      opts.host = &argv[x][7];
    } else if (!strncmp(argv[x], "--port=", 7)) {
      opts.port = atoi(&argv[x][7]);
    } else if (!strncmp(argv[x], "--username=", 11)) {
      opts.username = &argv[x][11];
    } else if (!strncmp(argv[x], "--password=", 11)) {
      opts.password = &argv[x][11];
      fprintf(stderr, "WARNING: The --password option will cause the password to appear in the system process list. Use --password-env for better security.\n");
    } else if (!strncmp(argv[x], "--password-env=", 15)) {
      opts.password = getenv(&argv[x][15]);
      if (opts.password == nullptr) {
        throw invalid_argument("environment variable specified by --password-env is not set");
      }
    } else if (!strncmp(argv[x], "--filename=", 11)) {
      opts.start_filename = &argv[x][11];
    } else if (!strncmp(argv[x], "--position=", 11)) {
      opts.start_position = strtoull(&argv[x][11], nullptr, 0);
    } else if (!strncmp(argv[x], "--tag=", 6)) {
      string tag = &argv[x][6];
      size_t equals_pos = tag.find('=');
      if (equals_pos != string::npos) {
        opts.constant_tags.emplace(tag.substr(0, equals_pos), tag.substr(equals_pos + 1));
      } else {
        opts.constant_tags.emplace(std::move(tag), "");
      }
    } else {
      throw invalid_argument("unknown option");
    }
  }

  EventAsync::Base base;
  generate_binlog_stats(base, opts);
  base.run();
  return 0;
}
