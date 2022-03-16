#include "Client.hh"

#include <stdio.h>

#include <phosg/Encoding.hh>
#include <phosg/Time.hh>

using namespace std;



namespace EventAsync::Memcache {

Client::Client(Base& base, const char* hostname, uint16_t port)
  : base(base),
    hostname(hostname),
    port(port),
    fd(-1) { }

Task<void> Client::connect() {
  if (this->fd.is_open()) {
    co_return;
  }
  this->fd = co_await this->base.connect(this->hostname, this->port);
}

Task<void> Client::quit() {
  if (!this->fd.is_open()) {
    co_return;
  }

  CommandHeader header;
  header.opcode = Command::Quit;
  header.key_size = 0;
  header.body_size = 0;
  header.byteswap();

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  co_await buf.write(this->fd);

  close(this->fd);
  this->fd = -1;
}

void Client::assert_conn_open() {
  if (!this->fd.is_open()) {
    throw runtime_error("cannot execute command on non-open connection");
  }
}



Task<CommandHeader> Client::read_response_header(Buffer& buf,
    uint16_t expected_error_code1, uint16_t expected_error_code2) {
  co_await buf.read_to(this->fd, sizeof(CommandHeader));
  auto header = buf.remove<CommandHeader>();
  if (header.magic != 0x81) {
    throw runtime_error("server responded to binary command with non-binary response");
  }
  header.byteswap();
  co_await buf.read_to(this->fd, header.body_size);
  if (header.status != ResponseStatus::OK) {
    if ((header.status != expected_error_code1) &&
        (header.status != expected_error_code2)) {
      string error_str = buf.remove(header.body_size);
      throw runtime_error(string_printf("server sent error %hu: %s",
          header.status, error_str.c_str()));
    } else {
      buf.drain(header.body_size); // skip the error message
    }
  }
  co_return header;
}



Task<Client::GetResult> Client::get(
    const void* key, size_t size, uint32_t expiration_secs) {
  this->assert_conn_open();

  CommandHeader header;
  header.opcode = expiration_secs ? Command::GetAndTouch : Command::Get;
  header.extras_size = expiration_secs ? 4 : 0;
  header.key_size = size;
  header.body_size = size + header.extras_size;
  header.byteswap();

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  if (expiration_secs) {
    expiration_secs = bswap32(expiration_secs);
    buf.add_reference(&expiration_secs, 4);
  }
  buf.add_reference(key, size);
  co_await buf.write(this->fd);

  header = co_await this->read_response_header(buf,
      ResponseStatus::KEY_NOT_FOUND);
  if (header.status) {
    co_return {.key_found = false};
  }

  if (header.extras_size != 4) {
    throw runtime_error("server responded to GET without flags");
  }
  uint32_t flags = buf.remove_u32b();
  string data = buf.remove(header.body_size - header.extras_size);
  co_return {
      .value = move(data),
      .cas = header.cas,
      .flags = flags,
      .key_found = true};
}

Task<bool> Client::set(
    const void* key,
    size_t key_size,
    const void* value,
    size_t value_size,
    uint32_t flags,
    uint32_t expiration_secs,
    uint64_t cas) {
  return this->write_key(Command::Set, ResponseStatus::KEY_EXISTS, 0, key,
      key_size, value, value_size, flags, expiration_secs, cas);
}

Task<bool> Client::add(
    const void* key,
    size_t key_size,
    const void* value,
    size_t value_size,
    uint32_t flags,
    uint32_t expiration_secs) {
  return this->write_key(Command::Add, ResponseStatus::KEY_EXISTS, 0, key,
      key_size, value, value_size, flags, expiration_secs, 0);
}

Task<bool> Client::replace(
    const void* key,
    size_t key_size,
    const void* value,
    size_t value_size,
    uint32_t flags,
    uint32_t expiration_secs,
    uint64_t cas) {
  return this->write_key(Command::Replace, ResponseStatus::KEY_NOT_FOUND,
      ResponseStatus::KEY_EXISTS, key, key_size, value, value_size, flags,
      expiration_secs, cas);
}

Task<bool> Client::write_key(
    Command command,
    uint16_t expected_error_code1,
    uint16_t expected_error_code2,
    const void* key,
    size_t key_size,
    const void* value,
    size_t value_size,
    uint32_t flags,
    uint32_t expiration_secs,
    uint64_t cas) {
  this->assert_conn_open();

  CommandHeader header;
  header.opcode = command;
  header.extras_size = 8;
  header.key_size = key_size;
  header.body_size = 8 + key_size + value_size;
  header.cas = cas;
  header.byteswap();

  uint32_t extras[2];
  extras[0] = bswap32(flags);
  extras[1] = bswap32(expiration_secs);

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  buf.add_reference(extras, 8);
  buf.add_reference(key, key_size);
  buf.add_reference(value, value_size);
  co_await buf.write(this->fd);

  header = co_await this->read_response_header(buf, expected_error_code1,
      expected_error_code2);
  if (header.status) {
    co_return false;
  }
  if (header.body_size) {
    throw runtime_error("write command returned response data after header");
  }
  co_return true;
}

Task<bool> Client::delete_key(const void* key, size_t key_size, uint64_t cas) {
  this->assert_conn_open();

  CommandHeader header;
  header.opcode = Command::Delete;
  header.key_size = key_size;
  header.body_size = key_size;
  header.cas = cas;
  header.byteswap();

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  buf.add_reference(key, key_size);
  co_await buf.write(this->fd);

  header = co_await this->read_response_header(buf,
      ResponseStatus::KEY_EXISTS);
  if (header.status) {
    co_return false;
  }
  if (header.body_size) {
    throw runtime_error("delete command returned response data after header");
  }
  co_return true;
}

Task<uint64_t> Client::increment(
    const void* key,
    size_t key_size,
    uint64_t delta,
    uint64_t initial_value,
    uint32_t expiration_secs,
    bool decrement) {
  this->assert_conn_open();

  CommandHeader header;
  header.opcode = decrement ? Command::Decrement : Command::Increment;
  header.extras_size = 0x14;
  header.key_size = key_size;
  header.body_size = 0x14 + key_size;
  header.byteswap();

  if (initial_value == 0xFFFFFFFFFFFFFFFF) {
    initial_value = delta;
  }
  struct {
    uint64_t delta;
    uint64_t initial_value;
    uint32_t expiration_secs;
  } __attribute__((packed)) extras = {
    bswap64(delta), bswap64(initial_value), bswap32(expiration_secs)};

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  buf.add_reference(&extras, sizeof(extras));
  buf.add_reference(key, key_size);
  co_await buf.write(this->fd);

  header = co_await this->read_response_header(buf,
      ResponseStatus::KEY_NOT_FOUND);
  if (header.status) {
    throw out_of_range("key not found");
  }
  if (header.body_size != 8) {
    throw runtime_error("incr/decr command returned response data after header");
  }
  co_return buf.remove_u64b();
}

Task<void> Client::append(
    const void* key,
    size_t key_size,
    const void* value,
    size_t value_size,
    bool prepend) {
  this->assert_conn_open();

  CommandHeader header;
  header.opcode = prepend ? Command::Prepend : Command::Append;
  header.key_size = key_size;
  header.body_size = key_size + value_size;
  header.byteswap();

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  buf.add_reference(key, key_size);
  buf.add_reference(value, value_size);
  co_await buf.write(this->fd);

  // TODO: which error codes should be expected here?
  co_await this->read_response_header(buf);
}

Task<void> Client::flush(uint32_t expiration_secs) {
  this->assert_conn_open();

  CommandHeader header;
  header.opcode = Command::Flush;
  header.extras_size = 4;
  header.key_size = 0;
  header.body_size = 4;
  header.byteswap();
  expiration_secs = bswap32(expiration_secs);

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  buf.add_reference(&expiration_secs, sizeof(expiration_secs));
  co_await buf.write(this->fd);

  co_await this->read_response_header(buf);
}

Task<uint64_t> Client::noop() {
  this->assert_conn_open();

  CommandHeader header;
  header.opcode = Command::NoOp;
  header.extras_size = 0;
  header.key_size = 0;
  header.body_size = 0;
  header.byteswap();

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  uint64_t start_usecs = now();
  co_await buf.write(this->fd);

  co_await this->read_response_header(buf);
  co_return now() - start_usecs;
}

Task<string> Client::version() {
  this->assert_conn_open();

  CommandHeader header;
  header.opcode = Command::Version;
  header.key_size = 0;
  header.body_size = 0;
  header.byteswap();

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  co_await buf.write(this->fd);

  header = co_await this->read_response_header(buf);
  co_return buf.remove(header.body_size);
}

Task<unordered_map<string, string>> Client::stats(
    const void* key, size_t key_size) {
  this->assert_conn_open();

  CommandHeader header;
  header.opcode = Command::Stat;
  header.key_size = key_size;
  header.body_size = key_size;
  header.byteswap();

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  if (key) {
    buf.add_reference(key, key_size);
  }
  co_await buf.write(this->fd);

  unordered_map<string, string> ret;
  for (;;) {
    header = co_await this->read_response_header(buf);
    if (header.extras_size != 0) {
      throw runtime_error("stats response contained unhandled extra data");
    }
    if (header.body_size == 0) {
      break;
    }
    string key = buf.remove(header.key_size);
    string value = buf.remove(header.body_size - header.key_size);
    ret.emplace(move(key), move(value));
  }
  co_return ret;
}

Task<void> Client::touch(
    const void* key, size_t size, uint32_t expiration_secs) {
  this->assert_conn_open();

  CommandHeader header;
  header.opcode = Command::Touch;
  header.extras_size = 4;
  header.key_size = size;
  header.body_size = size + 4;
  header.byteswap();
  expiration_secs = bswap32(expiration_secs);

  Buffer buf(this->base);
  buf.add_reference(&header, sizeof(header));
  buf.add_reference(&expiration_secs, 4);
  buf.add_reference(key, size);
  co_await buf.write(this->fd);

  // TODO: which error codes should be expected here?
  co_await this->read_response_header(buf);
}

} // namespace EventAsync::MySQL
