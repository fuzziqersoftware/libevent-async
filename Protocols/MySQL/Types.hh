#pragma once

#include <stdint.h>

#include <string>
#include <phosg/Filesystem.hh>
#include <event-async/AsyncTask.hh>
#include <event-async/EventBase.hh>
#include <event-async/EvDNSBase.hh>
#include <event-async/EvBuffer.hh>

#include "ProtocolBuffer.hh"



namespace EventAsync::MySQL {

enum CapFlag {
  LongPassword               = 0x00000001,
  FoundRows                  = 0x00000002,
  LongFlag                   = 0x00000004,
  ConnectWithDB              = 0x00000008,
  NoSchema                   = 0x00000010,
  Compress                   = 0x00000020,
  ODBC                       = 0x00000040,
  LocalFiles                 = 0x00000080,
  IgnoreSpace                = 0x00000100,
  Protocol41                 = 0x00000200,
  Interactive                = 0x00000400,
  SupportsSSL                = 0x00000800,
  IgnoreBrokenPipeSignal     = 0x00001000, // only used client-side apparently
  Transactions               = 0x00002000,
  SecureConnection           = 0x00008000,
  MultiStatements            = 0x00010000,
  MultiResults               = 0x00020000,
  PSMultiResults             = 0x00040000,
  PluginAuth                 = 0x00080000,
  ConnectAttrs               = 0x00100000,
  PluginAuthLenencClientData = 0x00200000,
  CanHandleExpiredPasswords  = 0x00400000,
  SessionTrack               = 0x00800000,
  DeprecateEOF               = 0x01000000,
};

enum StatusFlag {
  InTransaction         = 0x0001,
  Autocommit            = 0x0002,
  MoreResultsExist      = 0x0008,
  NoGoodIndexUsed       = 0x0010,
  NoIndexUsed           = 0x0020,
  CursorExists          = 0x0040,
  LastRowSent           = 0x0080,
  DatabaseDropped       = 0x0100,
  NoBackslashEscapes    = 0x0200,
  MetadataChanged       = 0x0400,
  QueryWasSlow          = 0x0800,
  PSOutParams           = 0x1000,
  InReadOnlyTransaction = 0x2000,
  SessionStateChanged   = 0x4000,
};

enum Command {
  Sleep            = 0x00,
  Quit             = 0x01,
  InitDB           = 0x02,
  Query            = 0x03,
  FieldList        = 0x04,
  CreateDB         = 0x05,
  DropDB           = 0x06,
  Refresh          = 0x07,
  Shutdown         = 0x08,
  Statistics       = 0x09,
  ProcessInfo      = 0x0A,
  Connect          = 0x0B,
  ProcessKill      = 0x0C,
  Debug            = 0x0D,
  Ping             = 0x0E,
  Time             = 0x0F,
  DelayedInsert    = 0x10,
  ChangeUser       = 0x11,
  BinlogDump       = 0x12,
  TableDump        = 0x13,
  ConnectOut       = 0x14,
  RegisterSlave    = 0x15,
  StmtPrepare      = 0x16,
  StmtExecute      = 0x17,
  StmtSendLongData = 0x18,
  StmtClose        = 0x19,
  StmtReset        = 0x1A,
  SetOption        = 0x1B,
  StmtFetch        = 0x1C,
  Daemon           = 0x1D,
  BinlogDumpGTID   = 0x1E,
  ResetConnection  = 0x1F,
};

enum ColumnType {
  T_DECIMAL    = 0x00,
  T_TINYINT    = 0x01,
  T_SMALLINT   = 0x02,
  T_INT        = 0x03,
  T_FLOAT      = 0x04,
  T_DOUBLE     = 0x05,
  T_NULL       = 0x06,
  T_TIMESTAMP  = 0x07,
  T_BIGINT     = 0x08,
  T_MEDIUMINT  = 0x09,
  T_DATE       = 0x0A,
  T_TIME       = 0x0B,
  T_DATETIME   = 0x0C,
  T_YEAR       = 0x0D,
  T_VARCHAR    = 0x0F,
  T_BIT        = 0x10,
  T_NEWDECIMAL = 0xF6,
  T_ENUM       = 0xF7,
  T_SET        = 0xF8,
  T_TINYBLOB   = 0xF9,
  T_MEDIUMBLOB = 0xFA,
  T_LONGBLOB   = 0xFB,
  T_BLOB       = 0xFC,
  T_VAR_STRING = 0xFD,
  T_STRING     = 0xFE,
  T_GEOMETRY   = 0xFF,
};

struct DateTimeValue {
  uint16_t years;
  uint8_t months;
  uint8_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint32_t usecs;

  DateTimeValue(const std::string& str);
  std::string str() const;
};

struct TimeValue {
  bool is_negative;
  uint32_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint32_t usecs;

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
  std::string
>;

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

struct ResultSet {
  std::vector<ColumnDefinition> columns;
  std::vector<std::vector<Value>> rows;
  uint64_t affected_rows;
  uint64_t insert_id;
  uint16_t status_flags;
  uint16_t warning_count;

  void print(FILE* stream) const;
};

} // namespace EventAsync::MySQL
