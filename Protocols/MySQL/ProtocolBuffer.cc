#include "ProtocolBuffer.hh"

#include <stdio.h>
#include <event2/buffer.h>

#include <phosg/Strings.hh>

using namespace std;



namespace EventAsync::MySQL {

AsyncTask<uint8_t> ProtocolBuffer::read_command(int fd) {
  if (this->get_length() != 0) {
    throw logic_error("attempted to read command into non-empty buffer");
  }
  co_await this->read_to(fd, 4);
  size_t command_length = this->remove_u24();
  uint8_t command_seq = this->remove_u8();
  co_await this->read_to(fd, command_length);
  co_return move(command_seq);
}

AsyncTask<void> ProtocolBuffer::write_command(int fd, uint8_t seq) {
  ProtocolBuffer send_buf(this->base);
  send_buf.add_u24(this->get_length());
  send_buf.add_u8(seq);
  send_buf.add_buffer(*this);
  co_await send_buf.write(fd);
}




uint32_t ProtocolBuffer::remove_u24() {
  uint32_t v = 0;
  this->remove_exactly(&v, 3);
  return v;
}

uint64_t ProtocolBuffer::remove_u48() {
  uint64_t v = 0;
  this->remove_exactly(&v, 6);
  return v;
}

uint64_t ProtocolBuffer::remove_varint() {
  uint8_t v = 0;
  this->remove_exactly(&v, 1);
  if (v < 0xFB) {
    return v;
  } else if (v == 0xFB) {
    throw runtime_error("length-encoded int is invalid (0xFB)");
  } else if (v == 0xFC) {
    return this->remove_u16();
  } else if (v == 0xFD) {
    return this->remove_u24();
  } else if (v == 0xFE) {
    return this->remove_u64();
  } else if (v == 0xFF) {
    throw runtime_error("length-encoded int is invalid (0xFF)");
  }
  return v;
}

void ProtocolBuffer::add_u24(uint32_t v) {
  this->add(&v, 3);
}

void ProtocolBuffer::add_u48(uint64_t v) {
  this->add(&v, 6);
}

void ProtocolBuffer::add_varint(uint64_t v) {
  if (v <= 0xFA) {
    this->add_u8(v);
  } else if (v <= 0xFFFF) {
    this->add_u8(0xFC);
    this->add_u16(v);
  } else if (v <= 0xFFFFFF) {
    this->add_u8(0xFD);
    this->add_u24(v);
  } else {
    this->add_u8(0xFE);
    this->add_u64(v);
  }
}



string ProtocolBuffer::remove_string0() {
  return this->readln(EVBUFFER_EOL_NUL);
}

string ProtocolBuffer::remove_var_string() {
  return this->remove(this->remove_varint());
}

string ProtocolBuffer::remove_string_eof() {
  return this->remove(this->get_length());
}

void ProtocolBuffer::add_string0(const string& str) {
  this->add(str.c_str(), str.size() + 1);
}

void ProtocolBuffer::add_var_string(const string& str) {
  this->add_varint(str.size());
  this->add(str);
}

void ProtocolBuffer::add_zeroes(size_t count) {
  for (; count >= 8; count -= 8) {
    this->add_u64(0);
  }
  if (count & 4) {
    this->add_u32(0);
  }
  if (count & 2) {
    this->add_u16(0);
  }
  if (count & 1) {
    this->add_u8(0);
  }
}

} // namespace EventAsync::MySQL
