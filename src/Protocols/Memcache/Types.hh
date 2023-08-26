#pragma once

#include <stdint.h>

namespace EventAsync::Memcache {

// Values used in the CommandHeader::status field
enum ResponseStatus {
  OK = 0x0000,
  KEY_NOT_FOUND = 0x0001,
  KEY_EXISTS = 0x0002,
  VALUE_TOO_LARGE = 0x0003,
  INVALID_ARGUMENTS = 0x0004,
  NOT_STORED = 0x0005,
  NON_NUMERIC_INCR_DECR = 0x0006,
  VIRTUAL_BUCKET_NOT_LOCAL = 0x0007,
  AUTHENTICATION_ERROR = 0x0008,
  AUTHENTICATION_CONTINUE = 0x0009,
  UNKNOWN_COMMAND = 0x0081,
  OUT_OF_MEMORY = 0x0082,
  NOT_SUPPORTED = 0x0083,
  INTERNAL_ERROR = 0x0084,
  BUSY = 0x0085,
  TEMPORARY_FAILURE = 0x0086,
};

enum Command {
  Get = 0x00,
  Set = 0x01,
  Add = 0x02,
  Replace = 0x03,
  Delete = 0x04,
  Increment = 0x05,
  Decrement = 0x06,
  Quit = 0x07,
  Flush = 0x08,
  GetQ = 0x09,
  NoOp = 0x0A,
  Version = 0x0B,
  GetK = 0x0C,
  GetKQ = 0x0D,
  Append = 0x0E,
  Prepend = 0x0F,
  Stat = 0x10,
  SetQ = 0x11,
  AddQ = 0x12,
  ReplaceQ = 0x13,
  DeleteQ = 0x14,
  IncrementQ = 0x15,
  DecrementQ = 0x16,
  QuitQ = 0x17,
  FlushQ = 0x18,
  AppendQ = 0x19,
  PrependQ = 0x1A,
  Verbosity = 0x1B,
  Touch = 0x1C,
  GetAndTouch = 0x1D,
  GetAndTouchQ = 0x1E,
  SASLListMechs = 0x20,
  SASLAuth = 0x21,
  SASLStep = 0x22,
  RGet = 0x30,
  RSet = 0x31,
  RSetQ = 0x32,
  RAppend = 0x33,
  RAppendQ = 0x34,
  RPrepend = 0x35,
  RPrependQ = 0x36,
  RDelete = 0x37,
  RDeleteQ = 0x38,
  RIncr = 0x39,
  RIncrQ = 0x3A,
  RDecr = 0x3B,
  RDecrQ = 0x3C,
  SetVBucket = 0x3D,
  GetVBucket = 0x3E,
  DelVBucket = 0x3F,
  TAPConnect = 0x40,
  TAPMutation = 0x41,
  TAPDelete = 0x42,
  TAPFlush = 0x43,
  TAPOpaque = 0x44,
  TAPVBucketSet = 0x45,
  TAPCheckpointStart = 0x46,
  TAPCheckpointEnd = 0x47,
};

struct CommandHeader {
  uint8_t magic; // 0x80 = request, 0x81 = response
  uint8_t opcode;
  uint16_t key_size;
  uint8_t extras_size;
  uint8_t data_type;
  union {
    uint16_t virtual_bucket_id; // used in requests
    uint16_t status; // used in responses
  };
  uint32_t body_size;
  uint32_t opaque;
  uint64_t cas;

  CommandHeader();

  void byteswap();
} __attribute__((packed));

} // namespace EventAsync::Memcache
