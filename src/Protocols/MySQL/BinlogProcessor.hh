#pragma once

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ProtocolBuffer.hh"
#include "Types.hh"

namespace EventAsync::MySQL {

enum BinlogEventType {
  UNKNOWN_EVENT = 0,
  START_EVENT_V3 = 1,
  QUERY_EVENT = 2,
  STOP_EVENT = 3,
  ROTATE_EVENT = 4,
  INTVAR_EVENT = 5,
  LOAD_EVENT = 6,
  SLAVE_EVENT = 7,
  CREATE_FILE_EVENT = 8,
  APPEND_BLOCK_EVENT = 9,
  EXEC_LOAD_EVENT = 10,
  DELETE_FILE_EVENT = 11,
  NEW_LOAD_EVENT = 12,
  RAND_EVENT = 13,
  USER_VAR_EVENT = 14,
  FORMAT_DESCRIPTION_EVENT = 15,
  XID_EVENT = 16,
  BEGIN_LOAD_QUERY_EVENT = 17,
  EXECUTE_LOAD_QUERY_EVENT = 18,
  TABLE_MAP_EVENT = 19,
  WRITE_ROWS_EVENTv0 = 20,
  UPDATE_ROWS_EVENTv0 = 21,
  DELETE_ROWS_EVENTv0 = 22,
  WRITE_ROWS_EVENTv1 = 23,
  UPDATE_ROWS_EVENTv1 = 24,
  DELETE_ROWS_EVENTv1 = 25,
  INCIDENT_EVENT = 26,
  HEARTBEAT_EVENT = 27,
  IGNORABLE_EVENT = 28,
  ROWS_QUERY_EVENT = 29,
  WRITE_ROWS_EVENTv2 = 30,
  UPDATE_ROWS_EVENTv2 = 31,
  DELETE_ROWS_EVENTv2 = 32,
  GTID_EVENT = 33,
  ANONYMOUS_GTID_EVENT = 34,
  PREVIOUS_GTIDS_EVENT = 35,
  TRANSACTION_CONTEXT_EVENT = 36,
  VIEW_CHANGE_EVENT = 37,
  XA_PREPARE_LOG_EVENT = 38,
  PARTIAL_UPDATE_ROWS_EVENT = 39,
  TRANSACTION_PAYLOAD_EVENT = 40,
};

const char* name_for_binlog_event_type(uint8_t type);

struct BinlogTableInfo {
  std::string database_name;
  std::string table_name;
  struct ColumnInfo {
    uint8_t type;
    std::string type_meta;
    bool nullable;
  };
  std::vector<ColumnInfo> columns;
};

struct BinlogEventHeader {
  uint32_t timestamp;
  uint8_t type;
  uint32_t server_id;
  uint32_t length;
  uint32_t end_position;
  uint16_t flags;
} __attribute__((packed));

struct BinlogTableMapEvent {
  BinlogEventHeader header;
  uint64_t table_id;
  uint16_t flags;
  std::shared_ptr<const BinlogTableInfo> ti;
};

struct BinlogRowsEvent {
  BinlogEventHeader header;

  enum class WriteType {
    INSERT,
    UPDATE,
    DELETE,
  };
  WriteType write_type;

  uint64_t table_id;
  std::shared_ptr<const BinlogTableInfo> ti;
  uint16_t flags;
  std::string extra_data; // unparsed

  struct RowChange {
    size_t pre_bytes;
    size_t post_bytes;
    std::vector<Value> pre;
    std::vector<Value> post;
  };
  std::vector<RowChange> rows;
};

struct BinlogQueryEvent {
  BinlogEventHeader header;
  uint32_t conn_id;
  uint32_t exec_time_secs;
  std::string database_name;
  uint16_t error_code;
  std::string status_vars; // unparsed
  std::string query;
};

struct BinlogRotateEvent {
  BinlogEventHeader header;
  std::string next_filename;
  uint64_t next_position;
};

struct BinlogXidEvent {
  BinlogEventHeader header;
  uint64_t xid;
};

struct BinlogFormatDescriptionEvent {
  BinlogEventHeader header;
  uint16_t version;
  std::string server_version;
  uint32_t timestamp;
  uint8_t header_length;
  std::string event_header_lengths;
};

class BinlogProcessor {
public:
  BinlogProcessor();
  ~BinlogProcessor() = default;

  static const BinlogEventHeader* get_event_header(const std::string& data);

  BinlogTableMapEvent parse_table_map_event(const std::string& data);
  BinlogRowsEvent parse_rows_event(const std::string& data);
  BinlogQueryEvent parse_query_event(const std::string& data);
  BinlogRotateEvent parse_rotate_event(const std::string& data);
  BinlogXidEvent parse_xid_event(const std::string& data);
  BinlogFormatDescriptionEvent parse_format_description_event(const std::string& data);
  BinlogEventHeader parse_unknown_event(const std::string& data);

private:
  std::string filename;
  uint64_t position;
  std::unordered_map<uint64_t, std::shared_ptr<BinlogTableInfo>> table_map;

  std::vector<Value> read_row_data(
      ProtocolStringReader& r, std::shared_ptr<const BinlogTableInfo> ti);
  static size_t metadata_bytes_for_column_type(uint8_t type);
  static Value read_cell_data(
      StringReader& r, const BinlogTableInfo::ColumnInfo& ci);
  static uint32_t read_datetime_fractional_part(
      StringReader& r, uint8_t precision);
};

} // namespace EventAsync::MySQL
