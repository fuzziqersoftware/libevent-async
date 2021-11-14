#pragma once

#include <stdint.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <phosg/Filesystem.hh>
#include "../../Task.hh"
#include "../../EventBase.hh"
#include "../../EvDNSBase.hh"
#include "../../EvBuffer.hh"

#include "Types.hh"



namespace EventAsync::Memcache {

class Client {
public:
  Client(EventBase& base, const char* hostname, uint16_t port);
  ~Client() = default;

  // Opens the connection. After constructing a Client object, you must
  // co_await client.connect() before calling any other methods on it.
  Task<void> connect();

  // Closes the connection. Does nothing if the connection isn't open.
  Task<void> quit();

  struct GetResult {
    std::string value;
    uint64_t cas;
    uint32_t flags;
    bool key_found;
  };

  // TODO: implement pipelined and/or multi versions of most of these functions

  // Reads a key. If the key is not found, returns a GetResult with key_found =
  // false. If an expiration time is given, resets the expiration time on the
  // key as well.
  Task<GetResult> get(
      const void* key, size_t size, uint32_t expiration_secs = 0);

  // Writes a key.
  Task<bool> set(
      const void* key,
      size_t key_size,
      const void* value,
      size_t value_size,
      uint32_t flags = 0,
      uint32_t expiration_secs = 0,
      uint64_t cas = 0);

  // Writes a key, but only if it does not already exist. Returns true if the
  // key was written.
  Task<bool> add(
      const void* key,
      size_t key_size,
      const void* value,
      size_t value_size,
      uint32_t flags = 0,
      uint32_t expiration_secs = 0);

  // Writes a key, but only if it already exists. Returns true if the key was
  // written.
  Task<bool> replace(
      const void* key,
      size_t key_size,
      const void* value,
      size_t value_size,
      uint32_t flags = 0,
      uint32_t expiration_secs = 0,
      uint64_t cas = 0);

  // Deletes a key. Returns true if the key was deleted.
  Task<bool> delete_key(const void* key, size_t key_size, uint64_t cas = 0);

  // Increment or decrement a key's value.
  Task<uint64_t> increment(
      const void* key,
      size_t key_size,
      uint64_t delta = 1,
      // If initial_value is not given, this special value causes the client to
      // use the delta as the initial value.
      uint64_t initial_value = 0xFFFFFFFFFFFFFFFF,
      uint32_t expiration_secs = 0,
      bool decrement = false);

  // Append or prepend data to a key's value.
  Task<void> append(
      const void* key,
      size_t key_size,
      const void* value,
      size_t value_size,
      bool prepend = false);

  // Delete all keys from the server.
  Task<void> flush(uint32_t expiration_secs = 0);

  // Ping the server. Returns the number of microseconds between the send and
  // receive. Note that this timing may be affected by the event loop's load.
  Task<uint64_t> noop();

  // Returns the server's version.
  Task<std::string> version();

  // Returns statistics, optionally for a specific key or prefix.
  Task<std::unordered_map<std::string, std::string>> stats(
      const void* key = nullptr, size_t key_size = 0);

  // Resets the expiration time on a key.
  Task<void> touch(const void* key, size_t size, uint32_t expiration_secs);

private:
  EventBase& base;
  std::string hostname;
  uint16_t port;

  scoped_fd fd;

  Task<CommandHeader> read_response_header(EvBuffer& buf,
      uint16_t expected_error_code1 = 0, uint16_t expected_error_code2 = 0);

  // Used for set, add, and replace
  Task<bool> write_key(
      Command command,
      uint16_t expected_error_code1,
      uint16_t expected_error_code2,
      const void* key,
      size_t key_size,
      const void* value,
      size_t value_size,
      uint32_t flags,
      uint32_t expiration_secs,
      uint64_t cas);

  void assert_conn_open();
};

} // namespace EventAsync::MySQL
