#include "Client.hh"

#include <stdio.h>
#include <event2/buffer.h>

#include <phosg/Hash.hh>
#include <phosg/Random.hh>
#include <phosg/Strings.hh>

#include "ProtocolBuffer.hh"

using namespace std;



namespace EventAsync::MySQL {

Client::Client(
    Base& base,
    const char* hostname,
    uint16_t port,
    const char* username,
    const char* password)
  : base(base),
    hostname(hostname),
    port(port),
    username(username),
    password(password),
    fd(-1),
    next_seq(0),
    binlog_read_state(BinlogReadState::NOT_READING),
    expected_binlog_seq(0) { }

Task<void> Client::connect() {
  if (this->fd.is_open()) {
    co_return;
  }
  this->fd = co_await this->base.connect(this->hostname, this->port);
  co_await this->initial_handshake();
}



Task<void> Client::read_command(ProtocolBuffer& buf) {
  uint8_t seq = co_await buf.read_command(this->fd);
  if (seq != this->next_seq) {
    throw runtime_error("server sent out-of-sequence commands");
  }
  this->next_seq++;
}

Task<void> Client::write_command(ProtocolBuffer& buf) {
  co_await buf.write_command(this->fd, this->next_seq++);
}

void Client::reset_seq() {
  this->next_seq = 0;
}



string Client::compute_auth_response(
    const string& auth_plugin_name,
    const string& auth_plugin_data,
    const string& password) {
  if (auth_plugin_name == "caching_sha2_password") {
    // The basic idea is:
    // resp = xor(sha256(password), sha256(sha256(sha256(password)) + nonce))
    string password_sha256 = sha256(password);
    string password_sha256_sha256 = sha256(password_sha256);
    string hash_with_nonce = sha256(password_sha256_sha256 + auth_plugin_data);
    string result(0x20, '\0');
    for (size_t x = 0; x < result.size(); x++) {
      result[x] = password_sha256[x] ^ hash_with_nonce[x];
    }
    return result;

  } else {
    throw runtime_error("unsupported auth plugin: " + auth_plugin_name);
  }
}

Task<void> Client::initial_handshake() {
  ProtocolBuffer buf(this->base);
  this->reset_seq();

  // Read the contents of the initial handshake
  co_await this->read_command(buf);
  if (buf.remove_u8() != 10) {
    throw runtime_error("unrecognized protocol version");
  }
  this->server_version = buf.remove_string0();
  this->connection_id = buf.remove_u32();
  string auth_challenge_data = buf.remove(8);
  buf.remove_u8(); // unused
  this->server_cap_flags = buf.remove_u16();
  this->charset = buf.remove_u8();
  this->status_flags = buf.remove_u16();
  this->server_cap_flags |= static_cast<uint32_t>(buf.remove_u16()) << 16;
  uint8_t auth_plugin_data_length = buf.remove_u8();
  buf.drain(10); // unused
  size_t remaining_auth_data_length = auth_plugin_data_length - 8;
  if (remaining_auth_data_length < 13) {
    remaining_auth_data_length = 13;
  }
  auth_challenge_data += buf.remove(remaining_auth_data_length);
  auth_challenge_data.resize(auth_plugin_data_length);
  string auth_plugin_name = buf.remove_string0();
  if (buf.get_length()) {
    throw logic_error("not all data in initial handshake was consumed");
  }

  // Undocumented behavior: if the auth challenge ends with a \0, strip it
  // off before passing it to the auth plugin
  if (auth_challenge_data.back() == '\0') {
    auth_challenge_data.pop_back();
  }

  // Send an appropriate reply
  string auth_response_data = this->compute_auth_response(
      auth_plugin_name, auth_challenge_data, this->password);

  // Capability flags we use: LongPassword, Protocol41, Transactions,
  // SecureConnection, MultiResults, PluginAuth, PluginAuthLenencClientData,
  // DeprecateEOF
  static const uint32_t client_cap_flags = 0x012AA201;
  buf.add_u32(client_cap_flags);
  buf.add_u32(0xFFFFFFFF); // max_allowed_packet
  buf.add_u8(0xFF); // charset
  buf.add_zeroes(23); // unused
  buf.add_string0(this->username);
  buf.add_var_string(auth_response_data);
  buf.add_string0(auth_plugin_name);
  co_await this->write_command(buf);

  // We expect to get an OK_Packet or ERR_Packet after this.
  for (;;) {
    co_await this->read_command(buf);
    uint8_t response_command = buf.remove_u8();

    if (response_command == 0x00) { // OK
      break;

    } else if (response_command == 0x01) { // auth more data
      if (auth_plugin_name == "caching_sha2_password") {
        uint8_t command = buf.remove_u8();
        if (command == 3) {
          // Fast auth success - in this case the server will send an OK
          // immediately after; we don't have to do anything here

        } else if (command == 4) {
          // "perform full authentication" - seems like it just returns the
          // password as a string0.
          if (buf.get_length()) {
            throw runtime_error("auth extra data not fully consumed");
          }

          // TODO: support this when we support SSL, but only if SSL has already
          // been enabled on the connection.
          throw runtime_error("server requested plaintext password, but connection is not encrypted");
          buf.add_string0(this->password);
          co_await this->write_command(buf);

        } else {
          throw runtime_error("unrecognized extra auth data command (caching_sha2_password)");
        }
      } else {
        throw runtime_error(
            "extra auth data command unimplemented for plugin " + auth_plugin_name);
      }

    } else if (response_command == 0xFF) { // ERR
      uint16_t error_code = buf.remove_u16();
      buf.drain(1); // sqlstate marker ('#')
      string sqlstate = buf.remove(5);
      string message = buf.remove_string_eof();
      throw runtime_error(string_printf("authentication failed: (%hu) %s",
          error_code, message.c_str()));

    } else {
      throw runtime_error("unrecognized command during authentication phase");
    }
  }
}

Task<void> Client::quit() {
  this->assert_conn_open();
  this->reset_seq();

  ProtocolBuffer buf(this->base);
  buf.add_u8(Command::Quit);
  co_await this->write_command(buf);
  this->fd.close();
}

static Value parse_value(ColumnType type, string&& value) {
  switch (type) {
    case ColumnType::T_TINYINT:
    case ColumnType::T_SMALLINT:
    case ColumnType::T_MEDIUMINT:
    case ColumnType::T_INT:
    case ColumnType::T_BIGINT:
    case ColumnType::T_YEAR:
      if (value.at(0) == '-') {
        return stoll(value, nullptr, 10);
      } else {
        return stoull(value, nullptr, 10);
      }
    case ColumnType::T_FLOAT:
      return stof(value);
    case ColumnType::T_DOUBLE:
      return stod(value);
    case ColumnType::T_NULL:
      return nullptr;
    case ColumnType::T_DATE:
    case ColumnType::T_DATETIME:
    case ColumnType::T_TIMESTAMP:
      return DateTimeValue(value);
    case ColumnType::T_TIME:
      return TimeValue(value);
    case ColumnType::T_BIT:
    case ColumnType::T_STRING:
    case ColumnType::T_VAR_STRING:
    case ColumnType::T_VARCHAR:
    case ColumnType::T_TINYBLOB:
    case ColumnType::T_BLOB:
    case ColumnType::T_MEDIUMBLOB:
    case ColumnType::T_LONGBLOB:
    case ColumnType::T_DECIMAL:
    case ColumnType::T_NEWDECIMAL:
    case ColumnType::T_ENUM:
    case ColumnType::T_SET:
    case ColumnType::T_GEOMETRY:
      return move(value);
    default:
      throw runtime_error("invalid value type");
  }
}

Task<void> Client::change_db(const string& db_name) {
  this->assert_conn_open();
  this->reset_seq();

  ProtocolBuffer buf(this->base);
  buf.add_u8(Command::InitDB);
  buf.add(db_name);
  co_await this->write_command(buf);
  co_await this->expect_ok();
}



Task<ResultSet> Client::query(const std::string& sql, bool rows_as_dicts) {
  this->assert_conn_open();
  this->reset_seq();

  ProtocolBuffer buf(this->base);
  buf.add_u8(Command::Query);
  buf.add(sql);
  co_await this->write_command(buf);

  // The first response command speecifies either that the query completed (if
  // no result set is returned), that the query failed, or how many columns are
  // in the returned result set
  ResultSet res;
  co_await this->read_command(buf);
  uint8_t response_command = buf.copyout_u8();
  if (response_command == 0x00) { // OK
    buf.remove_u8();
    res.affected_rows = buf.remove_varint();
    res.insert_id = buf.remove_varint();
    res.status_flags = buf.remove_u16();
    res.warning_count = buf.remove_u16();
    co_return move(res);
  } else if (response_command == 0xFF) { // ERR
    this->parse_error_body(buf);
  } else if (response_command == 0xFB) { // LOCAL INFILE request
    throw runtime_error("LOCAL INFILE requests are not implemented");
  }

  // If we get here, then a result set is being returned. Set up the return
  // structures appropriately.
  if (rows_as_dicts) {
    res.rows = vector<unordered_map<string, Value>>();
  } else {
    res.rows = vector<vector<Value>>();
  }

  // The column definitions are sent as individual commands first.
  uint64_t column_count = buf.remove_varint();
  while (res.columns.size() < column_count) {
    co_await this->read_command(buf);

    ColumnDefinition& def = res.columns.emplace_back();
    def.catalog_name = buf.remove_var_string();
    def.database_name = buf.remove_var_string();
    def.table_name = buf.remove_var_string();
    def.original_table_name = buf.remove_var_string();
    def.column_name = buf.remove_var_string();
    def.original_column_name = buf.remove_var_string();
    if (buf.remove_varint() != 0x0C) {
      throw runtime_error("column metadata has incorrect fixed-length header");
    }
    def.charset = buf.remove_u16();
    def.max_value_length = buf.remove_u32();
    def.type = static_cast<ColumnType>(buf.remove_u8());
    def.flags = buf.remove_u16();
    def.decimals = buf.remove_u8();
    buf.remove_u16(); // unused
  }

  // After the column definitions, each row is sent as an individual command.
  for (;;) {
    co_await this->read_command(buf);

    if (buf.copyout_u8() == 0xFE) {
      buf.remove_u8();
      res.affected_rows = 0;
      res.insert_id = 0;
      res.warning_count = buf.remove_u16();
      res.status_flags = buf.remove_u16();
      co_return move(res);
    }

    if (rows_as_dicts) {
      auto& row = get<vector<unordered_map<string, Value>>>(res.rows)
          .emplace_back();
      for (const auto& column_def : res.columns) {
        if (buf.copyout_u8() == 0xFB) {
          buf.remove_u8();
          row.emplace(column_def.column_name, nullptr);
        } else {
          row.emplace(
              column_def.column_name,
              parse_value(column_def.type, buf.remove_var_string()));
        }
      }

    } else {
      auto& row = get<vector<vector<Value>>>(res.rows).emplace_back();
      for (const auto& column_def : res.columns) {
        if (buf.copyout_u8() == 0xFB) {
          buf.remove_u8();
          row.emplace_back(nullptr);
        } else {
          row.emplace_back(parse_value(column_def.type, buf.remove_var_string()));
        }
      }
    }
  }
}



Task<void> Client::read_binlogs(
    const string& filename, size_t position, uint32_t server_id, bool block) {
  this->assert_conn_open();

  co_await this->query(
      "SET @master_binlog_checksum = 'ALL', @source_binlog_checksum = 'ALL'");

  if (server_id == 0) {
    server_id = random_object<uint32_t>();
  }

  this->reset_seq();

  ProtocolBuffer buf(this->base);
  buf.add_u8(Command::BinlogDump);
  buf.add_u32(position);
  buf.add_u16(block ? 0x0000 : 0x0001);
  buf.add_u32(server_id);
  buf.add(filename);
  co_await this->write_command(buf);

  this->binlog_read_state = BinlogReadState::READING_FIRST_EVENT;
  this->expected_binlog_seq = 1;
}

Task<string> Client::get_binlog_event() {
  this->assert_conn_open();
  if (this->binlog_read_state == BinlogReadState::NOT_READING) {
    throw logic_error("get_binlog_event called before read_binlogs or after out_of_range");
  }

  ProtocolBuffer buf(this->base);
  co_await this->read_command(buf);

  uint8_t command = buf.remove_u8();
  if (command == 0xFE) {
    this->binlog_read_state = BinlogReadState::NOT_READING;
    throw out_of_range("end of binlog stream");
  } else if (command == 0) {
    string ret = buf.remove_string_eof();
  //   It appears the first event is always an artificial ROTATE_EVENT and does
    // NOT have a checksum; all subsequent ROTATE_EVENTs (even if artificial) DO
    // have checksums.
    // TODO: Is this a correct description of the actual behavior? Do we need
    // fancier logic to handle these?
    if (this->binlog_read_state != BinlogReadState::READING_FIRST_EVENT) {
      ret.resize(ret.size() - 4);
    }
    this->binlog_read_state = BinlogReadState::READING_EVENT;
    co_return ret;
  } else {
    throw runtime_error("binlog event does not begin with OK byte");
  }
}



void Client::assert_conn_open() {
  if (!this->fd.is_open()) {
    throw runtime_error("cannot execute command on non-open connection");
  }
}

void Client::parse_error_body(ProtocolBuffer& buf) {
  uint16_t error_code = buf.remove_u16();
  string sqlstate = buf.remove(6); // '#' + 5-char sqlstate
  string message = buf.remove_string_eof();
  throw runtime_error(string_printf("(%hu; %s) %s",
      error_code, sqlstate.c_str(), message.c_str()));
}

Task<void> Client::expect_ok(uint8_t expected_seq) {
  ProtocolBuffer buf(this->base);

  co_await this->read_command(buf);

  uint8_t response_command = buf.remove_u8();
  if (response_command == 0x00) { // OK
    co_return;
  } else if (response_command == 0xFF) { // ERR
    this->parse_error_body(buf);
  } else {
    throw runtime_error("unrecognized response when OK or ERR expected");
  }
}

} // namespace EventAsync::MySQL
