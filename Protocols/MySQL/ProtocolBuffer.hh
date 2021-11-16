#pragma once

#include <stdint.h>

#include <string>
#include "../../Buffer.hh"



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
  uint32_t remove_u24();
  uint64_t remove_u48();
  uint64_t remove_varint();
  void add_u24(uint32_t v);
  void add_u48(uint64_t v);
  void add_varint(uint64_t v);

  // String types
  std::string remove_string0();
  std::string remove_var_string();
  std::string remove_string_eof();
  void add_string0(const std::string& str);
  void add_var_string(const std::string& str);
  void add_zeroes(size_t count);
};

} // namespace EventAsync::MySQL
