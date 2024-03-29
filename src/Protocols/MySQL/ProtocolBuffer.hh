#pragma once

#include <stdint.h>

#include "../../Buffer.hh"
#include <phosg/Strings.hh>
#include <string>

namespace EventAsync::MySQL {

class ProtocolBuffer : public Buffer {
public:
  using Buffer::Buffer;
  virtual ~ProtocolBuffer() = default;

  // Command sending/receiving
  Task<uint8_t> read_command(int fd); // returns sequence number
  Task<void> write_command(int fd, uint8_t seq);

  // Integer types (Buffer already provides 8/16/32/64)
  // Warning: these functions do not consider the endianness of the system; it
  // appears the MySQL protocol uses little-endian integers, so these will only
  // work on little-endian systems.
  uint32_t remove_u24l();
  uint64_t remove_u48l();
  uint64_t remove_varint();
  void add_u24l(uint32_t v);
  void add_u48l(uint64_t v);
  void add_varint(uint64_t v);

  // String types
  std::string remove_string0();
  std::string remove_var_string();
  std::string remove_string_eof();
  void add_string0(const std::string& str);
  void add_var_string(const std::string& str);
  void add_zeroes(size_t count);
};

class ProtocolStringReader : public StringReader {
public:
  using StringReader::StringReader;
  virtual ~ProtocolStringReader() = default;

  uint64_t get_varint();
  std::string get_string0();
  std::string get_var_string();
  std::string get_string_eof();
  std::vector<bool> get_bitmask(size_t num_bits);
};

} // namespace EventAsync::MySQL
