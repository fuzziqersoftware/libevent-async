#pragma once

#include <stdint.h>

#include <string>
#include <phosg/Filesystem.hh>
#include "../../Task.hh"
#include "../../Base.hh"
#include "../../DNSBase.hh"
#include "../../Buffer.hh"

#include "ProtocolBuffer.hh"
#include "Types.hh"



namespace EventAsync::MySQL {

class Client {
public:
  Client(
      Base& base,
      const char* hostname,
      uint16_t port,
      const char* username,
      const char* password);
  ~Client() = default;

  // Opens the connection. After constructing a Client object, you must
  // co_await client.connect() before calling any other methods on it.
  Task<void> connect();

  // Closes the connection. Does nothing if the connection isn't open.
  Task<void> quit();

  // Sets the current default database.
  Task<void> change_db(const std::string& db_name);

  // Runs a SQL query.
  Task<ResultSet> query(const std::string& sql);

  // Starts a binlog stream. To read binlogs, call this method to start reading,
  // then call get_binlog_event infinitely many times or until it throws
  // out_of_range.
  Task<void> read_binlogs(
      // Binlog filename and position to start reading from.
      const std::string& filename,
      size_t position,
      // Reader's server_id. If zero, the client picks a random nonzero value.
      uint32_t server_id = 0,
      // Blocking mode. See description of get_binlog_event() below.
      bool block = true);

  // Returns the next binlog event from the server's stream. If read_binlogs was
  // called with block=true and there are no more events available, this method
  // waits for the server to send another one. If read_binlogs was called with
  // block=false, this method throws out_of_range when there are no more events
  // available from the server.
  Task<std::string> get_binlog_event();

private:
  Base& base;
  std::string hostname;
  uint16_t port;
  std::string username;
  std::string password;

  scoped_fd fd;
  uint8_t next_seq;
  std::string server_version;
  uint32_t connection_id;
  uint32_t server_cap_flags;
  uint8_t charset;
  uint16_t status_flags;

  bool reading_binlogs;
  uint8_t expected_binlog_seq;

  Task<void> read_command(ProtocolBuffer& buf);
  Task<void> write_command(ProtocolBuffer& buf);
  void reset_seq();

  static std::string compute_auth_response(
      const std::string& auth_plugin_name,
      const std::string& auth_plugin_data,
      const std::string& password);
  Task<void> initial_handshake();

  void assert_conn_open();
  void parse_error_body(ProtocolBuffer& buf);

  Task<void> expect_ok(uint8_t expected_seq = 1);
};

} // namespace EventAsync::MySQL
