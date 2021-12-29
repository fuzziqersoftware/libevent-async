#include "Types.hh"

#include <phosg/Encoding.hh>



namespace EventAsync::Memcache {

CommandHeader::CommandHeader()
  : magic(0x80),
    // opcode must be set manually by the caller
    // key_length must be set manually by the caller
    extras_size(0),
    data_type(0),
    virtual_bucket_id(0),
    // body_length must be set manually by the caller
    opaque(0),
    cas(0) { }

void CommandHeader::byteswap() {
  this->key_size = bswap16(this->key_size);
  this->status = bswap16(this->status);
  this->body_size = bswap32(this->body_size);
  this->opaque = bswap32(this->opaque);
  this->cas = bswap64(this->cas);
}

} // namespace EventAsync::Memcache
