#include "BinlogProcessor.hh"

#include <stdio.h>
#include <event2/buffer.h>

#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "ProtocolBuffer.hh"

using namespace std;



namespace EventAsync::MySQL {

const char* name_for_binlog_event_type(uint8_t type) {
  static const vector<const char*> names({
    "UNKNOWN_EVENT",
    "START_EVENT_V3",
    "QUERY_EVENT",
    "STOP_EVENT",
    "ROTATE_EVENT",
    "INTVAR_EVENT",
    "LOAD_EVENT",
    "SLAVE_EVENT",
    "CREATE_FILE_EVENT",
    "APPEND_BLOCK_EVENT",
    "EXEC_LOAD_EVENT",
    "DELETE_FILE_EVENT",
    "NEW_LOAD_EVENT",
    "RAND_EVENT",
    "USER_VAR_EVENT",
    "FORMAT_DESCRIPTION_EVENT",
    "XID_EVENT",
    "BEGIN_LOAD_QUERY_EVENT",
    "EXECUTE_LOAD_QUERY_EVENT",
    "TABLE_MAP_EVENT",
    "WRITE_ROWS_EVENTv0",
    "UPDATE_ROWS_EVENTv0",
    "DELETE_ROWS_EVENTv0",
    "WRITE_ROWS_EVENTv1",
    "UPDATE_ROWS_EVENTv1",
    "DELETE_ROWS_EVENTv1",
    "INCIDENT_EVENT",
    "HEARTBEAT_EVENT",
    "IGNORABLE_EVENT",
    "ROWS_QUERY_EVENT",
    "WRITE_ROWS_EVENTv2",
    "UPDATE_ROWS_EVENTv2",
    "DELETE_ROWS_EVENTv2",
    "GTID_EVENT",
    "ANONYMOUS_GTID_EVENT",
    "PREVIOUS_GTIDS_EVENT",
    "TRANSACTION_CONTEXT_EVENT",
    "VIEW_CHANGE_EVENT",
    "XA_PREPARE_LOG_EVENT",
    "PARTIAL_UPDATE_ROWS_EVENT",
    "TRANSACTION_PAYLOAD_EVENT",
  });
  try {
    return names.at(type);
  } catch (const out_of_range&) {
    return "<INVALID_EVENT_TYPE>";
  }
}



uint32_t BinlogProcessor::read_datetime_fractional_part(
    StringReader& r, uint8_t precision) {
  switch (precision) {
    case 0:
      return 0;
    case 1:
      return r.get_u8() * 10000;
    case 2:
      return r.get_u8() * 10000;
    case 3:
      return r.get_u16b() * 100;
    case 4:
      return r.get_u16b() * 100;
    case 5:
      return r.get_u24b();
    case 6:
      return r.get_u24b();
    default:
      throw runtime_error("invalid time-like precision specifier");
  }
}

Value BinlogProcessor::read_cell_data(
    StringReader& r, const BinlogTableInfo::ColumnInfo& ci) {
  switch (ci.type) {
    case ColumnType::T_NULL:
      return nullptr;

    case ColumnType::T_TINYINT:
      return static_cast<uint64_t>(r.get_u8());
    case ColumnType::T_SMALLINT:
      return static_cast<uint64_t>(r.get_u16l());
    case ColumnType::T_MEDIUMINT:
      return static_cast<uint64_t>(r.get_u24l());
    case ColumnType::T_INT:
      return static_cast<uint64_t>(r.get_u32l());
    case ColumnType::T_BIGINT:
      return r.get_u64l();
    case ColumnType::T_FLOAT:
      return r.get_f32l();
    case ColumnType::T_DOUBLE:
      return r.get_f64l();

    case ColumnType::T_TINYBLOB:
    case ColumnType::T_MEDIUMBLOB:
    case ColumnType::T_LONGBLOB:
    case ColumnType::T_BLOB:
    case ColumnType::T_GEOMETRY:
    case ColumnType::T_JSON: {
      if (ci.type_meta.size() != 1) {
        throw runtime_error("invalid type options");
      }
      size_t size;
      switch (ci.type_meta[0]) {
        case 1:
          size = r.get_u8();
          break;
        case 2:
          size = r.get_u16l();
          break;
        case 3:
          size = r.get_u24l();
          break;
        case 4:
          size = r.get_u32l();
          break;
        default:
          throw runtime_error("invalid blob-type meta-length");
      }
      return r.read(size);
    }

    case ColumnType::T_YEAR:
      return static_cast<uint64_t>(r.get_u8());

    case ColumnType::T_ENUM:
    case ColumnType::T_SET:
    case ColumnType::T_STRING: {
      if (ci.type_meta.size() != 2) {
        throw runtime_error("invalid type options");
      }
      uint8_t subtype = ci.type_meta[0];
      if (subtype == ColumnType::T_SET || subtype == ColumnType::T_ENUM) {
        return r.read(ci.type_meta[1]);
      } else {
        // Someone working the MySQL server was too clever for their own good here
        uint16_t max_display_width =
            (((static_cast<uint16_t>(ci.type_meta[0]) << 4) & 0x300) ^ 0x300) +
            static_cast<uint8_t>(ci.type_meta[1]);
        if (max_display_width > 255) {
          return r.read(r.get_u16l());
        } else {
          return r.read(r.get_u8());
        }
      }
    }

    case ColumnType::T_VARCHAR: {
      if (ci.type_meta.size() != 2) {
        throw runtime_error("invalid type options");
      }
      if (*reinterpret_cast<const uint16_t*>(ci.type_meta.data()) > 255) {
        return r.read(r.get_u16l());
      } else {
        return r.read(r.get_u8());
      }
    }

    case ColumnType::T_TIMESTAMP:
      return static_cast<uint64_t>(r.get_u32l()) * 1000000;

    case ColumnType::T_TIMESTAMP2: {
      uint64_t ret = static_cast<uint64_t>(r.get_u32l());
      if (ci.type_meta.size() != 1) {
        throw logic_error("invalid type options");
      }
      if (ci.type_meta[0]) {
        // TODO: parse the fractional part too, duh
        string fractional_part = r.read((ci.type_meta[0] + 1) / 2);
        fprintf(stderr, "warning: skipping TIMESTAMP2 fractional part:\n");
        print_data(stderr, fractional_part);
      }
      return ret;
    }

    case ColumnType::T_DATETIME2: {
      if (ci.type_meta.size() != 1) {
        throw logic_error("invalid type options");
      }

      uint64_t z = r.get_u32b();
      z = (z << 8) | r.get_u8();

      // T_DATETIME2 values are 40-bit values laid out like this:
      // LLLLLLLL LLLLLLLL LLDDDDDH HHHHMMMM MMSSSSSS
      // where L is a combination of years and months (see implementation), and
      // D, H, M, S are self-explanatory.

      DateTimeValue ret;
      ret.years = ((z >> 22) - 131073) / 13;
      ret.months = ((z >> 22) + 7) % 13;
      ret.days = (z >> 17) & 0x1F;
      ret.hours = (z >> 12) & 0x1F;
      ret.minutes = (z >> 6) & 0x3F;
      ret.seconds = z & 0x3F;
      ret.usecs = read_datetime_fractional_part(r, ci.type_meta[0]);
      return ret;
    }

    case ColumnType::T_TIME2: {
      if (ci.type_meta.size() != 1) {
        throw logic_error("invalid type options");
      }

      // T_TIME2 values are 24-bit values laid out like this:
      // QHHHHHHH HHHHMMMM MMSSSSSS
      // If Q is 1, the value is positive, and H, M, and S directly encode the
      // time. If Q is 0, the value is negative, and ~H, ~M, and (~S + 1) encode
      // the time. Since S cannot use its full range of values (59 < 63), we
      // don't have to worry about the +1 overflowing into the M field, so we
      // just do it before splitting out the fields.

      uint32_t z = r.get_u24b();
      TimeValue ret;
      ret.is_negative = !(z & 0x800000);
      if (ret.is_negative) {
        z = (~z) + 1;
      }
      ret.hours = (z >> 12) & 0x3FF;
      ret.minutes = (z >> 6) & 0x3F;
      ret.seconds = z & 0x3F;
      ret.usecs = read_datetime_fractional_part(r, ci.type_meta[0]);
      return ret;
    }

    // NOTE: T_BIT is handled in a way that implies that type_meta should not
    // be empty, but it looks like type_meta is supposed ot always be empty. See
    // https://github.com/mysql/mysql-server/blob/3e90d07c3578e4da39dc1bce73559bbdf655c28c/libbinlogevents/src/binary_log_funcs.cpp
    case ColumnType::T_BIT:
    case ColumnType::T_BOOL:
    case ColumnType::T_VAR_STRING:
    case ColumnType::T_DECIMAL: // metadata[0] data bytes
    case ColumnType::T_NEWDECIMAL: // metadata[0, 1] = [precision, scale]; see decimal_binary_size in binary_log_funcs.cpp
    case ColumnType::T_DATE: // 3 data bytes
    case ColumnType::T_TIME: // 3 data bytes
    case ColumnType::T_DATETIME: // 8 data bytes
    case ColumnType::T_NEWDATE: // 3 data bytes
    default:
      throw runtime_error("unimplemented or invalid column type");
  }
}

size_t BinlogProcessor::metadata_bytes_for_column_type(uint8_t type) {
  switch (type) {
    case ColumnType::T_NULL:
      throw logic_error("T_NULL cannot be specified in a place where metadata is needed");

    case ColumnType::T_DATE:
    case ColumnType::T_DATETIME:
    case ColumnType::T_TIME:
    case ColumnType::T_TIMESTAMP:
    case ColumnType::T_TINYINT:
    case ColumnType::T_SMALLINT:
    case ColumnType::T_MEDIUMINT:
    case ColumnType::T_INT:
    case ColumnType::T_BIGINT:
    case ColumnType::T_YEAR:
    case ColumnType::T_BOOL:
    case ColumnType::T_VAR_STRING:
    case ColumnType::T_NEWDATE: // 3 data bytes
      return 0;

    case ColumnType::T_TINYBLOB:
    case ColumnType::T_BLOB:
    case ColumnType::T_MEDIUMBLOB:
    case ColumnType::T_LONGBLOB:
    case ColumnType::T_FLOAT:
    case ColumnType::T_DOUBLE:
    case ColumnType::T_GEOMETRY:
    case ColumnType::T_JSON:
    case ColumnType::T_TIMESTAMP2:
    case ColumnType::T_TIME2:
    case ColumnType::T_DATETIME2:
      return 1;

    case ColumnType::T_STRING:
    case ColumnType::T_VARCHAR:
    case ColumnType::T_DECIMAL:
    case ColumnType::T_NEWDECIMAL:
    case ColumnType::T_ENUM:
    case ColumnType::T_SET:
    case ColumnType::T_BIT:
      return 2;

    default:
      throw runtime_error("unimplemented or invalid column type");
  }
}

vector<Value> BinlogProcessor::read_row_data(
    ProtocolStringReader& r, shared_ptr<const BinlogTableInfo> ti) {
  vector<bool> columns_null = r.get_bitmask(ti->columns.size());
  vector<Value> row;
  for (size_t column_index = 0;
       column_index < ti->columns.size();
       column_index++) {
    const auto& ci = ti->columns[column_index];
    if (columns_null[column_index]) {
      if (!ci.nullable) {
        throw runtime_error("found null value in non-nullable column");
      }
      row.emplace_back(nullptr);
    } else {
      row.emplace_back(read_cell_data(r, ci));
    }
  }
  if (row.size() != ti->columns.size()) {
    throw runtime_error("row1 length does not match column count");
  }
  return row;
}



BinlogProcessor::BinlogProcessor() : filename("<missing-filename>"), position(4) { }

const BinlogEventHeader* BinlogProcessor::get_event_header(const string& data) {
  if (data.size() < sizeof(BinlogEventHeader)) {
    throw runtime_error("binlog event too small for header");
  }
  return reinterpret_cast<const BinlogEventHeader*>(data.data());
}

BinlogTableMapEvent BinlogProcessor::parse_table_map_event(const string& data) {
  shared_ptr<BinlogTableInfo> ti(new BinlogTableInfo());

  BinlogTableMapEvent ev;
  ProtocolStringReader r(data);
  ev.header = r.get<BinlogEventHeader>();
  if (ev.header.type != BinlogEventType::TABLE_MAP_EVENT) {
    throw logic_error("event is not a table map event");
  }
  ev.table_id = r.get_u48l();
  ev.flags = r.get_u16l(); // flags
  ti->database_name = r.read(r.get_u8());
  r.skip(1); // unused
  ti->table_name = r.read(r.get_u8());
  r.skip(1); // unused

  uint64_t num_columns = r.get_varint();
  string column_types;
  vector<bool> columns_nullable(num_columns);
  for (size_t x = 0; x < num_columns; x++) {
    column_types.push_back(r.get_u8());
  }
  string column_type_metas = r.get_var_string();
  columns_nullable = r.get_bitmask(num_columns);

  StringReader type_meta_r(column_type_metas);
  for (size_t column_index = 0; column_index < num_columns; column_index++) {
    auto& ci = ti->columns.emplace_back();
    ci.type = column_types[column_index];
    ci.type_meta = type_meta_r.read(metadata_bytes_for_column_type(ci.type));
    ci.nullable = columns_nullable[column_index];
  }
  if (!type_meta_r.eof()) {
    throw runtime_error("not all column type_meta was parsed");
  }

  // Uncomment to debug TABLE_MAP_EVENT parsing
  // fprintf(stderr, "/* MAP TABLE %s.%s AS %llu; */\n",
  //     ti->database_name.c_str(), ti->table_name.c_str(), table_id);
  // for (size_t x = 0; x < num_columns; x++) {
  //   const auto& ci = ti->columns[x];
  //   string meta_str = ci.type_meta.empty()
  //       ? "" : (" (meta " + format_data_string(ci.type_meta) + ")");
  //   fprintf(stderr, "/*   column %zu = %s%s%s */\n",
  //       x, name_for_column_type(ci.type), meta_str.c_str(),
  //       ci.nullable ? " NULL" : " NOT NULL");
  // }

  this->table_map[ev.table_id] = ti;
  this->position = ev.header.end_position;
  return ev;
}

BinlogRowsEvent BinlogProcessor::parse_rows_event(const string& data) {
  BinlogRowsEvent ev;
  ProtocolStringReader r(data);
  ev.header = r.get<BinlogEventHeader>();

  bool is_v2 =
      (ev.header.type == BinlogEventType::WRITE_ROWS_EVENTv2) ||
      (ev.header.type == BinlogEventType::UPDATE_ROWS_EVENTv2) ||
      (ev.header.type == BinlogEventType::DELETE_ROWS_EVENTv2);
  bool has_preimage =
      (ev.header.type == BinlogEventType::UPDATE_ROWS_EVENTv1) ||
      (ev.header.type == BinlogEventType::UPDATE_ROWS_EVENTv2);
  if ((ev.header.type == BinlogEventType::WRITE_ROWS_EVENTv0) ||
      (ev.header.type == BinlogEventType::WRITE_ROWS_EVENTv1) ||
      (ev.header.type == BinlogEventType::WRITE_ROWS_EVENTv2)) {
    ev.write_type = BinlogRowsEvent::WriteType::INSERT;
  } else if ((ev.header.type == BinlogEventType::UPDATE_ROWS_EVENTv0) ||
      (ev.header.type == BinlogEventType::UPDATE_ROWS_EVENTv1) ||
      (ev.header.type == BinlogEventType::UPDATE_ROWS_EVENTv2)) {
    ev.write_type = BinlogRowsEvent::WriteType::UPDATE;
  } else if ((ev.header.type == BinlogEventType::DELETE_ROWS_EVENTv0) ||
      (ev.header.type == BinlogEventType::DELETE_ROWS_EVENTv1) ||
      (ev.header.type == BinlogEventType::DELETE_ROWS_EVENTv2)) {
    ev.write_type = BinlogRowsEvent::WriteType::DELETE;
  } else {
    throw logic_error("event is not a rows event");
  }

  ev.table_id = r.get_u48l();
  if (ev.table_id == 0x000000FFFFFF) {
    // TODO: implement the correct behavior here
    throw logic_error("table map free during row events not supported");
  }
  ev.ti = this->table_map.at(ev.table_id);
  ev.flags = r.get_u16l(); // flags
  if (is_v2) {
    ev.extra_data = r.read(r.get_u16l() - 2);
  }
  uint64_t num_columns = r.get_varint();
  if (num_columns != ev.ti->columns.size()) {
    throw runtime_error("row event column count does not match table info from table map");
  }

  // TODO: do we need to support the case where some columns aren't present?
  for (size_t x = 0; x < (1 + has_preimage); x++) {
    for (bool p : r.get_bitmask(num_columns)) {
      if (!p) {
        throw runtime_error("one or more columns not present in row event");
      }
    }
  }

  while (!r.eof()) {
    auto& rc = ev.rows.emplace_back();
    size_t start_offset = r.where();
    switch (ev.write_type) {
      case BinlogRowsEvent::WriteType::INSERT:
        rc.post = this->read_row_data(r, ev.ti);
        rc.post_bytes = r.where() - start_offset;
        break;
      case BinlogRowsEvent::WriteType::UPDATE:
        if (has_preimage) {
          rc.pre = this->read_row_data(r, ev.ti);
          rc.pre_bytes = r.where() - start_offset;
          start_offset = r.where();
        }
        rc.post = this->read_row_data(r, ev.ti);
        rc.post_bytes = r.where() - start_offset;
        break;
      case BinlogRowsEvent::WriteType::DELETE:
        rc.pre = this->read_row_data(r, ev.ti);
        rc.pre_bytes = r.where() - start_offset;
        break;
    }
  }

  this->position = ev.header.end_position;
  return ev;
}

BinlogQueryEvent BinlogProcessor::parse_query_event(const string& data) {
  BinlogQueryEvent ev;
  ProtocolStringReader r(data);
  ev.header = r.get<BinlogEventHeader>();
  if (ev.header.type != BinlogEventType::QUERY_EVENT) {
    throw logic_error("event is not a query event");
  }
  ev.conn_id = r.get_u32l();
  ev.exec_time_secs = r.get_u32l();
  uint8_t database_name_length = r.get_u8();
  ev.error_code = r.get_u16l();
  ev.status_vars = r.read(r.get_u16l());
  ev.database_name = r.read(database_name_length);
  r.skip(1); // unused
  ev.query = r.get_string_eof();

  this->position = ev.header.end_position;
  return ev;
}

BinlogRotateEvent BinlogProcessor::parse_rotate_event(const string& data) {
  BinlogRotateEvent ev;
  ProtocolStringReader r(data);
  ev.header = r.get<BinlogEventHeader>();
  if (ev.header.type != BinlogEventType::ROTATE_EVENT) {
    throw logic_error("event is not a rotate event");
  }
  ev.next_position = r.get_u64l();
  ev.next_filename = r.get_string_eof();

  this->filename = ev.next_filename;
  this->position = ev.next_position;
  return ev;
}

BinlogXidEvent BinlogProcessor::parse_xid_event(const string& data) {
  BinlogXidEvent ev;
  ProtocolStringReader r(data);
  ev.header = r.get<BinlogEventHeader>();
  if (ev.header.type != BinlogEventType::XID_EVENT) {
    throw logic_error("event is not an xid event");
  }
  ev.xid = r.get_u64l();

  this->position = ev.header.end_position;
  return ev;
}

BinlogFormatDescriptionEvent BinlogProcessor::parse_format_description_event(
    const string& data) {
  BinlogFormatDescriptionEvent ev;
  ProtocolStringReader r(data);
  ev.header = r.get<BinlogEventHeader>();

  static const uint8_t expected_header_lengths[40] = {
    0x00, 0x0D, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x61, 0x00, 0x04, 0x1A, 0x08, 0x00,
    0x00, 0x00, 0x08, 0x08, 0x08, 0x02, 0x00, 0x00, 0x00, 0x0A,
    0x0A, 0x0A, 0x2A, 0x2A, 0x00, 0x12, 0x34, 0x00, 0x0A, 0x28,
  };
  ev.version = r.get_u16l();
  if (ev.version != 4) {
    throw runtime_error("binlog version is not 4");
  }

  ev.server_version = r.read(50);
  size_t zero_pos = ev.server_version.find('\0');
  if (zero_pos != string::npos) {
    ev.server_version.resize(zero_pos);
  }

  ev.timestamp = r.get_u32l();
  ev.header_length = r.get_u8();
  if (ev.header_length != 0x13) {
    throw runtime_error("incorrect header length for FORMAT_DESCRIPTION_EVENT");
  }
  ev.event_header_lengths = r.get_string_eof();
  if (memcmp(expected_header_lengths, ev.event_header_lengths.data(),
      ev.event_header_lengths.size() < sizeof(expected_header_lengths)
        ? ev.event_header_lengths.size()
        : sizeof(expected_header_lengths))) {
    throw runtime_error("header lengths in FORMAT_DESCRIPTION_EVENT do not match expectations");
  }

  this->position = ev.header.end_position;
  return ev;
}

BinlogEventHeader BinlogProcessor::parse_unknown_event(const string& data) {
  if (data.size() < sizeof(BinlogEventHeader)) {
    throw runtime_error("binlog event too small for header");
  }

  const auto* header = reinterpret_cast<const BinlogEventHeader*>(data.data());
  this->position = header->end_position;
  return *header;
}

} // namespace EventAsync::MySQL
