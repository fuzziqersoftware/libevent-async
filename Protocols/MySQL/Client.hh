#pragma once

#include <stdint.h>

#include <string>
#include <phosg/Filesystem.hh>
#include <event-async/AsyncTask.hh>
#include <event-async/EventBase.hh>
#include <event-async/EvDNSBase.hh>
#include <event-async/EvBuffer.hh>

#include "ProtocolBuffer.hh"
#include "Types.hh"



namespace EventAsync::MySQL {

class Client {
public:
  Client(
      EventBase& base,
      const char* hostname,
      uint16_t port,
      const char* username,
      const char* password);
  ~Client() = default;

  // Open/close the connection
  AsyncTask<void> connect();
  AsyncTask<void> quit();

  // Set the current database
  AsyncTask<void> change_db(const std::string& db_name);

  // Send a query
  AsyncTask<ResultSet> query(const std::string& sql);

  // Read binlogs - call read_binlogs to start reading, then call
  // get_binlog_event forever or until it throws out_of_range
  AsyncTask<void> read_binlogs(
      // Filename and position to start reading from
      const std::string& filename,
      size_t position,
      // Reader's server_id; if zero, client picks a random nonzero value
      uint32_t server_id = 0,
      // Blocking mode. If true, get_binlog_event() will block on the remote
      // side when there are no new events; if false, get_binlog_event() will
      // throw std::out_of_range when there are no new events.
      bool block = true);
  AsyncTask<std::string> get_binlog_event();

private:
  EventBase& base;
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

  AsyncTask<void> read_command(ProtocolBuffer& buf);
  AsyncTask<void> write_command(ProtocolBuffer& buf);
  void reset_seq();

  static std::string compute_auth_response(
      const std::string& auth_plugin_name,
      const std::string& auth_plugin_data,
      const std::string& password);
  AsyncTask<void> initial_handshake();

  void assert_conn_open();
  void parse_error_body(ProtocolBuffer& buf);

  AsyncTask<void> expect_ok(uint8_t expected_seq = 1);
};

} // namespace EventAsync::MySQL
