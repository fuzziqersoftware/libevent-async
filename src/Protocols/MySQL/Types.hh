#pragma once

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace EventAsync::MySQL {

enum CapFlag {
  LONG_PASSWORD = 0x00000001,
  FOUND_ROWS = 0x00000002,
  LONG_FLAG = 0x00000004,
  CONNECT_WITH_DB = 0x00000008,
  NO_SCHEMA = 0x00000010,
  COMPRESS = 0x00000020,
  ODBC = 0x00000040,
  LOCAL_FILES = 0x00000080,
  IGNORE_SPACE = 0x00000100,
  PROTOCOL_41 = 0x00000200,
  INTERACTIVE = 0x00000400,
  SUPPORTS_SSL = 0x00000800,
  IGNORE_BROKEN_PIPE_SIGNAL = 0x00001000, // only used client-side apparently
  TRANSACTIONS = 0x00002000,
  SECURE_CONNECTION = 0x00008000,
  MULTI_STATEMENTS = 0x00010000,
  MULTI_RESULTS = 0x00020000,
  PS_MULTI_RESULTS = 0x00040000,
  PLUGIN_AUTH = 0x00080000,
  CONNECT_ATTRS = 0x00100000,
  PLUGIN_AUTH_LENENC_CLIENT_DATA = 0x00200000,
  CAN_HANDLE_EXPIRED_PASSWORDS = 0x00400000,
  SESSION_TRACK = 0x00800000,
  DEPRECATE_EOF = 0x01000000,
};

enum StatusFlag {
  IN_TRANSACTION = 0x0001,
  AUTOCOMMIT = 0x0002,
  MORE_RESULTS_EXIST = 0x0008,
  NO_GOOD_INDEX_USED = 0x0010,
  NO_INDEX_USED = 0x0020,
  CURSOR_EXISTS = 0x0040,
  LAST_ROW_SENT = 0x0080,
  DATABASE_DROPPED = 0x0100,
  NO_BACKSLASH_ESCAPES = 0x0200,
  METADATA_CHANGED = 0x0400,
  QUERY_WAS_SLOW = 0x0800,
  PS_OUT_PARAMS = 0x1000,
  IN_READ_ONLY_TRANSACTION = 0x2000,
  SESSION_STATE_CHANGED = 0x4000,
};

enum Command {
  SLEEP = 0x00,
  QUIT = 0x01,
  INIT_DB = 0x02,
  QUERY = 0x03,
  FIELD_LIST = 0x04,
  CREATE_DB = 0x05,
  DROP_DB = 0x06,
  REFRESH = 0x07,
  SHUTDOWN = 0x08,
  STATISTICS = 0x09,
  PROCESS_INFO = 0x0A,
  CONNECT = 0x0B,
  PROCESS_KILL = 0x0C,
  DEBUG = 0x0D,
  PING = 0x0E,
  TIME = 0x0F,
  DELAYED_INSERT = 0x10,
  CHANGE_USER = 0x11,
  BINLOG_DUMP = 0x12,
  TABLE_DUMP = 0x13,
  CONNECT_OUT = 0x14,
  REGISTER_SLAVE = 0x15,
  STMT_PREPARE = 0x16,
  STMT_EXECUTE = 0x17,
  STMT_SEND_LONG_DATA = 0x18,
  STMT_CLOSE = 0x19,
  STMT_RESET = 0x1A,
  SET_OPTION = 0x1B,
  STMT_FETCH = 0x1C,
  DAEMON = 0x1D,
  BINLOG_DUMP_GTID = 0x1E,
  RESET_CONNECTION = 0x1F,
};

enum ColumnType {
  T_DECIMAL = 0x00,
  T_TINYINT = 0x01,
  T_SMALLINT = 0x02,
  T_INT = 0x03,
  T_FLOAT = 0x04,
  T_DOUBLE = 0x05,
  T_NULL = 0x06,
  T_TIMESTAMP = 0x07,
  T_BIGINT = 0x08,
  T_MEDIUMINT = 0x09,
  T_DATE = 0x0A,
  T_TIME = 0x0B,
  T_DATETIME = 0x0C,
  T_YEAR = 0x0D,
  T_NEWDATE = 0x0E,
  T_VARCHAR = 0x0F,
  T_BIT = 0x10,
  T_TIMESTAMP2 = 0x11,
  T_DATETIME2 = 0x12,
  T_TIME2 = 0x13,
  T_BOOL = 0xF4,
  T_JSON = 0xF5,
  T_NEWDECIMAL = 0xF6,
  T_ENUM = 0xF7,
  T_SET = 0xF8,
  T_TINYBLOB = 0xF9,
  T_MEDIUMBLOB = 0xFA,
  T_LONGBLOB = 0xFB,
  T_BLOB = 0xFC,
  T_VAR_STRING = 0xFD,
  T_STRING = 0xFE,
  T_GEOMETRY = 0xFF,
};

const char* name_for_column_type(uint8_t type);

struct DateTimeValue {
  uint16_t years;
  uint8_t months;
  uint8_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint32_t usecs;

  DateTimeValue() = default;
  DateTimeValue(const std::string& str);
  std::string str() const;
};

struct TimeValue {
  bool is_negative;
  uint32_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint32_t usecs;

  TimeValue() = default;
  TimeValue(const std::string& str);
  std::string str() const;
};

using Value = std::variant<
    // T_TINYINT, T_SMALLINT, T_MEDIUMINT, T_INT, T_BIGINT, T_YEAR
    uint64_t,
    int64_t,

    // T_FLOAT
    float,
    // T_DOUBLE
    double,

    // T_NULL (value is always nullptr)
    const void*,

    // T_DATE, T_DATETIME, T_TIMESTAMP
    DateTimeValue,
    // T_TIME
    TimeValue,

    // T_BIT, T_STRING, T_VAR_STRING, T_VARCHAR, T_TINYBLOB, T_BLOB, T_MEDIUMBLOB,
    // T_LONGBLOB, T_DECIMAL, T_NEWDECIMAL, T_ENUM, T_SET, T_GEOMETRY
    std::string>;

struct ColumnDefinition {
  std::string catalog_name;
  std::string database_name;
  std::string table_name;
  std::string original_table_name;
  std::string column_name;
  std::string original_column_name;
  uint16_t charset;
  uint32_t max_value_length;
  ColumnType type;
  uint16_t flags;
  uint8_t decimals;
};

std::string format_cell_value(const Value& cell);

struct ResultSet {
  std::vector<ColumnDefinition> columns;
  std::variant<
      std::vector<std::vector<Value>>,
      std::vector<std::unordered_map<std::string, Value>>>
      rows;

  uint64_t affected_rows;
  uint64_t insert_id;
  uint16_t status_flags;
  uint16_t warning_count;

  std::vector<std::vector<Value>>& rows_vecs();
  const std::vector<std::vector<Value>>& rows_vecs() const;
  std::vector<std::unordered_map<std::string, Value>>& rows_dicts();
  const std::vector<std::unordered_map<std::string, Value>>& rows_dicts() const;

  void print(FILE* stream) const;
};

} // namespace EventAsync::MySQL
