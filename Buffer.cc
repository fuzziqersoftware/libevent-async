#include "Buffer.hh"

#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>

using namespace std;
using namespace std::experimental;



namespace EventAsync {

Buffer::Buffer(Base& base)
  : base(base), buf(evbuffer_new()), owned(true) {
  if (!this->buf) {
    throw bad_alloc();
  }
}

Buffer::Buffer(Base& base, struct evbuffer* buf)
  : base(base), buf(buf), owned(false) { }

Buffer::Buffer(Buffer&& other) : base(other.base), buf(other.buf) {
  other.buf = nullptr;
  other.owned = false;
}

Buffer::~Buffer() {
  if (this->owned) {
    evbuffer_free(this->buf);
  }
}

void Buffer::enable_locking(void* lock) {
  if (evbuffer_enable_locking(this->buf, lock)) {
    throw runtime_error("evbuffer_enable_locking");
  }
}

void Buffer::lock() {
  evbuffer_lock(this->buf);
}

void Buffer::unlock() {
  evbuffer_unlock(this->buf);
}

size_t Buffer::get_length() const {
  return evbuffer_get_length(this->buf);
}

size_t Buffer::get_contiguous_space() const {
  return evbuffer_get_contiguous_space(this->buf);
}

void Buffer::expand(size_t size) {
  if (evbuffer_expand(this->buf, size)) {
    throw runtime_error("evbuffer_expand");
  }
}

void Buffer::add(const void* data, size_t size) {
  if (evbuffer_add(this->buf, data, size)) {
    throw runtime_error("evbuffer_add");
  }
}

void Buffer::add(const string& data) {
  this->add(data.data(), data.size());
}

size_t Buffer::add_printf(const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  int ret = evbuffer_add_vprintf(this->buf, fmt, va);
  va_end(va);
  if (ret < 0) {
    throw runtime_error("evbuffer_add_vprintf");
  }
  return ret;
}

size_t Buffer::add_vprintf(const char* fmt, va_list va) {
  int ret = evbuffer_add_vprintf(this->buf, fmt, va);
  if (ret < 0) {
    throw runtime_error("evbuffer_add_vprintf");
  }
  return ret;
}

void Buffer::add_buffer(struct evbuffer* src) {
  if (evbuffer_add_buffer(this->buf, src)) {
    throw runtime_error("evbuffer_add_buffer");
  }
}

void Buffer::add_buffer(Buffer& src) {
  if (evbuffer_add_buffer(this->buf, src.buf)) {
    throw runtime_error("evbuffer_add_buffer");
  }
}

size_t Buffer::remove_buffer(struct evbuffer* src, size_t size) {
  int ret = evbuffer_remove_buffer(this->buf, src, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove_buffer");
  }
  return ret;
}

size_t Buffer::remove_buffer(Buffer& src, size_t size) {
  int ret = evbuffer_remove_buffer(this->buf, src.buf, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove_buffer");
  }
  return ret;
}

void Buffer::prepend(const void* data, size_t size) {
  if (evbuffer_prepend(this->buf, data, size)) {
    throw runtime_error("evbuffer_prepend");
  }
}

void Buffer::prepend_buffer(struct evbuffer* src) {
  if (evbuffer_prepend_buffer(this->buf, src)) {
    throw runtime_error("evbuffer_prepend_buffer");
  }
}

void Buffer::prepend_buffer(Buffer& src) {
  if (evbuffer_prepend_buffer(this->buf, src.buf)) {
    throw runtime_error("evbuffer_prepend_buffer");
  }
}

uint8_t* Buffer::pullup(ssize_t size) {
  uint8_t* ret = evbuffer_pullup(this->buf, size);
  if (!ret) {
    throw runtime_error("evbuffer_pullup");
  }
  return ret;
}

void Buffer::drain(size_t size) {
  if (evbuffer_drain(this->buf, size)) {
    throw runtime_error("evbuffer_drain");
  }
}

void Buffer::drain_all() {
  this->drain(this->get_length());
}

size_t Buffer::remove(void* data, size_t size) {
  int ret = evbuffer_remove(this->buf, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove");
  }
  return ret;
}

string Buffer::remove(size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  data.resize(this->remove(const_cast<char*>(data.data()), size));
  return data;
}

void Buffer::remove_exactly(void* data, size_t size) {
  int ret = evbuffer_remove(this->buf, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove");
  } else if (ret < size) {
    throw runtime_error("not enough data in buffer");
  }
}

string Buffer::remove_exactly(size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  this->remove_exactly(const_cast<char*>(data.data()), size);
  return data;
}

size_t Buffer::copyout(void* data, size_t size) {
  ssize_t ret = evbuffer_copyout(this->buf, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_copyout");
  }
  return ret;
}

string Buffer::copyout(size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  data.resize(this->copyout(const_cast<char*>(data.data()), size));
  return data;
}

void Buffer::copyout_exactly(void* data, size_t size) {
  int ret = evbuffer_copyout(this->buf, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_copyout");
  } else if (ret < size) {
    throw runtime_error("not enough data in buffer");
  }
}

string Buffer::copyout_exactly(size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  this->copyout_exactly(const_cast<char*>(data.data()), size);
  return data;
}

size_t Buffer::copyout_from(const struct evbuffer_ptr* pos, void* data, size_t size) {
  ssize_t ret = evbuffer_copyout_from(this->buf, pos, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_copyout_from");
  }
  return ret;
}

string Buffer::copyout_from(const struct evbuffer_ptr* pos, size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  data.resize(this->copyout_from(pos, const_cast<char*>(data.data()), size));
  return data;
}

void Buffer::copyout_from_exactly(const struct evbuffer_ptr* pos, void* data, size_t size) {
  ssize_t ret = evbuffer_copyout_from(this->buf, pos, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_copyout_from");
  } else if (ret < size) {
    throw runtime_error("not enough data in buffer");
  }
}

string Buffer::copyout_from_exactly(const struct evbuffer_ptr* pos, size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  this->copyout_from_exactly(pos, const_cast<char*>(data.data()), size);
  return data;
}

void Buffer::add_u8(uint8_t v) {
  this->add<uint8_t>(v);
}

void Buffer::add_s8(int8_t v) {
  this->add<int8_t>(v);
}

void Buffer::add_u16(uint16_t v) {
  this->add<uint16_t>(v);
}

void Buffer::add_s16(int16_t v) {
  this->add<int16_t>(v);
}

void Buffer::add_u16r(uint16_t v) {
  this->add<uint16_t>(bswap16(v));
}

void Buffer::add_s16r(int16_t v) {
  this->add<int16_t>(bswap16(v));
}

void Buffer::add_u32(uint32_t v) {
  this->add<uint32_t>(v);
}

void Buffer::add_s32(int32_t v) {
  this->add<int32_t>(v);
}

void Buffer::add_u32r(uint32_t v) {
  this->add<uint32_t>(bswap32(v));
}

void Buffer::add_s32r(int32_t v) {
  this->add<int32_t>(bswap32(v));
}

void Buffer::add_u64(uint64_t v) {
  this->add<uint64_t>(v);
}

void Buffer::add_s64(int64_t v) {
  this->add<int64_t>(v);
}

void Buffer::add_u64r(uint64_t v) {
  this->add<uint64_t>(bswap64(v));
}

void Buffer::add_s64r(int64_t v) {
  this->add<int64_t>(bswap64(v));
}

uint8_t Buffer::remove_u8() {
  return this->remove<uint8_t>();
}

int8_t Buffer::remove_s8() {
  return this->remove<int8_t>();
}

uint16_t Buffer::remove_u16() {
  return this->remove<uint16_t>();
}

int16_t Buffer::remove_s16() {
  return this->remove<int16_t>();
}

uint16_t Buffer::remove_u16r() {
  return bswap16(this->remove<uint16_t>());
}

int16_t Buffer::remove_s16r() {
  return bswap16(this->remove<int16_t>());
}

uint32_t Buffer::remove_u32() {
  return this->remove<uint32_t>();
}

int32_t Buffer::remove_s32() {
  return this->remove<int32_t>();
}

uint32_t Buffer::remove_u32r() {
  return bswap32(this->remove<uint32_t>());
}

int32_t Buffer::remove_s32r() {
  return bswap32(this->remove<int32_t>());
}

uint64_t Buffer::remove_u64() {
  return this->remove<uint64_t>();
}

int64_t Buffer::remove_s64() {
  return this->remove<int64_t>();
}

uint64_t Buffer::remove_u64r() {
  return bswap64(this->remove<uint64_t>());
}

int64_t Buffer::remove_s64r() {
  return bswap64(this->remove<int64_t>());
}

uint8_t Buffer::copyout_u8() {
  return this->copyout<uint8_t>();
}

int8_t Buffer::copyout_s8() {
  return this->copyout<int8_t>();
}

uint16_t Buffer::copyout_u16() {
  return this->copyout<uint16_t>();
}

int16_t Buffer::copyout_s16() {
  return this->copyout<int16_t>();
}

uint16_t Buffer::copyout_u16r() {
  return bswap16(this->copyout<uint16_t>());
}

int16_t Buffer::copyout_s16r() {
  return bswap16(this->copyout<int16_t>());
}

uint32_t Buffer::copyout_u32() {
  return this->copyout<uint32_t>();
}

int32_t Buffer::copyout_s32() {
  return this->copyout<int32_t>();
}

uint32_t Buffer::copyout_u32r() {
  return bswap32(this->copyout<uint32_t>());
}

int32_t Buffer::copyout_s32r() {
  return bswap32(this->copyout<int32_t>());
}

uint64_t Buffer::copyout_u64() {
  return this->copyout<uint64_t>();
}

int64_t Buffer::copyout_s64() {
  return this->copyout<int64_t>();
}

uint64_t Buffer::copyout_u64r() {
  return bswap64(this->copyout<uint64_t>());
}

int64_t Buffer::copyout_s64r() {
  return bswap64(this->copyout<int64_t>());
}


string Buffer::readln(enum evbuffer_eol_style eol_style) {
  size_t bytes_read;
  char* ret = evbuffer_readln(this->buf, &bytes_read, eol_style);
  if (ret) {
    // TODO: find a way to keep the API clean without having to copy the string
    // again here
    string str(ret, bytes_read);
    free(ret);
    return str;
  } else {
    throw runtime_error("end of stream");
  }
}

struct evbuffer_ptr Buffer::search(const char* what, size_t size,
    const struct evbuffer_ptr* start) {
  return evbuffer_search(this->buf, what, size, start);
}
struct evbuffer_ptr Buffer::search_range(const char* what, size_t size,
    const struct evbuffer_ptr *start, const struct evbuffer_ptr *end) {
  return evbuffer_search_range(this->buf, what, size, start, end);
}
struct evbuffer_ptr Buffer::search_eol(struct evbuffer_ptr* start,
    size_t* bytes_found, enum evbuffer_eol_style eol_style) {
  return evbuffer_search_eol(this->buf, start, bytes_found, eol_style);
}

void Buffer::ptr_set(struct evbuffer_ptr* pos, size_t position,
    enum evbuffer_ptr_how how) {
  if (evbuffer_ptr_set(this->buf, pos, position, how)) {
    throw runtime_error("evbuffer_ptr_set");
  }
}

int Buffer::peek(ssize_t size, struct evbuffer_ptr* start_at,
    struct evbuffer_iovec* vec_out, int n_vec) {
  return evbuffer_peek(this->buf, size, start_at, vec_out, n_vec);
}

int Buffer::reserve_space(ev_ssize_t size, struct evbuffer_iovec* vec, int n_vecs) {
  return evbuffer_reserve_space(this->buf, size, vec, n_vecs);
}

void Buffer::commit_space(struct evbuffer_iovec* vec, int n_vecs) {
  if (evbuffer_commit_space(this->buf, vec, n_vecs)) {
    throw runtime_error("evbuffer_commit_space");
  }
}

void Buffer::add_reference(const string& data) {
  if (evbuffer_add_reference(this->buf, data.data(), data.size(), nullptr, nullptr)) {
    throw runtime_error("evbuffer_add_reference");
  }
}

void Buffer::add_reference(const void* data, size_t size,
    void (*cleanup_fn)(const void* data, size_t size, void* ctx), void* ctx) {
  if (evbuffer_add_reference(this->buf, data, size, cleanup_fn, ctx)) {
    throw runtime_error("evbuffer_add_reference");
  }
}

static void dispatch_cxx_cleanup_fn(const void* data, size_t size, void* ctx) {
  auto* fn = reinterpret_cast<function<void(const void* data, size_t size)>*>(ctx);
  (*fn)(data, size);
  delete fn;
}

void Buffer::add_reference(const void* data, size_t size,
    function<void(const void* data, size_t size)> cleanup_fn) {
  // TODO: can we do this without an extra allocation?
  auto* fn_copy = new function<void(const void* data, size_t size)>(cleanup_fn);
  int ret = evbuffer_add_reference(this->buf, data, size, dispatch_cxx_cleanup_fn, fn_copy);
  if (ret < 0) {
    delete fn_copy;
    throw runtime_error("evbuffer_add_reference");
  }
}

static void dispatch_delete_string(const void* data, size_t size, void* ctx) {
  delete reinterpret_cast<string*>(ctx);
}

void Buffer::add(string&& data) {
  string* s = new string(move(data));
  int ret = evbuffer_add_reference(this->buf, s->data(), s->size(),
      dispatch_delete_string, s);
  if (ret < 0) {
    data = move(*s);
    delete s;
    throw runtime_error("evbuffer_add_reference");
  }
}

void Buffer::add_file(int fd, off_t offset, size_t size) {
  if (evbuffer_add_file(this->buf, fd, offset, size)) {
    throw runtime_error("evbuffer_add_file");
  }
}

void Buffer::add_buffer_reference(struct evbuffer* other_buf) {
  if (evbuffer_add_buffer_reference(this->buf, other_buf)) {
    throw runtime_error("evbuffer_add_buffer_reference");
  }
}

void Buffer::add_buffer_reference(Buffer& other_buf) {
  if (evbuffer_add_buffer_reference(this->buf, other_buf.buf)) {
    throw runtime_error("evbuffer_add_buffer_reference");
  }
}

void Buffer::freeze(int at_front) {
  if (evbuffer_freeze(this->buf, at_front)) {
    throw runtime_error("evbuffer_freeze");
  }
}

void Buffer::unfreeze(int at_front) {
  if (evbuffer_unfreeze(this->buf, at_front)) {
    throw runtime_error("evbuffer_unfreeze");
  }
}

void Buffer::debug_print_contents(FILE* stream) {
  size_t size = this->get_length();
  if (size) {
    print_data(stream, this->copyout(size));
  }
}

Buffer::ReadAtMostAwaiter Buffer::read_atmost(evutil_socket_t fd, ssize_t size) {
  return ReadAtMostAwaiter(*this, fd, size);
}

Buffer::ReadExactlyAwaiter Buffer::read(
    evutil_socket_t fd, size_t size) {
  return ReadExactlyAwaiter(*this, fd, static_cast<ssize_t>(size));
}

Buffer::ReadExactlyAwaiter Buffer::read_to(
    evutil_socket_t fd, size_t size) {
  if (size <= this->get_length()) {
    return ReadExactlyAwaiter(*this, fd, 0);
  } else {
    return ReadExactlyAwaiter(*this, fd, size - this->get_length());
  }
}

Buffer::WriteAwaiter Buffer::write(evutil_socket_t fd, ssize_t size) {
  return WriteAwaiter(*this, fd, size);
}



Buffer::ReadAwaiter::ReadAwaiter(
    Buffer& buf,
    evutil_socket_t fd,
    ssize_t limit,
    void (*cb)(evutil_socket_t, short, void*))
  : buf(buf),
    event(this->buf.base, fd, EV_READ, cb, this),
    limit(limit),
    bytes_read(0),
    err(false),
    coro(nullptr) { }

bool Buffer::ReadAwaiter::await_ready() const noexcept {
  return (this->bytes_read >= this->limit);
}

void Buffer::ReadAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event.add();
}

Buffer::ReadAtMostAwaiter::ReadAtMostAwaiter(
    Buffer& buf,
    evutil_socket_t fd,
    ssize_t limit)
  : ReadAwaiter(buf, fd, limit, &ReadAtMostAwaiter::on_read_ready) { }

size_t Buffer::ReadAtMostAwaiter::await_resume() {
  if (this->err) {
    throw runtime_error("failed to read from fd");
  }
  return this->bytes_read;
}

void Buffer::ReadAtMostAwaiter::on_read_ready(evutil_socket_t fd, short what, void* ctx) {
  ReadAtMostAwaiter* aw = reinterpret_cast<ReadAtMostAwaiter*>(ctx);
  ssize_t bytes_read = evbuffer_read(aw->buf.buf, aw->event.get_fd(), aw->limit);
  aw->err = (bytes_read < 0);
  aw->bytes_read = bytes_read;
  aw->coro.resume();
}

Buffer::ReadExactlyAwaiter::ReadExactlyAwaiter(
    Buffer& buf,
    evutil_socket_t fd,
    size_t limit)
  : ReadAwaiter(buf, fd, limit, &ReadExactlyAwaiter::on_read_ready),
    eof(false) { }

void Buffer::ReadExactlyAwaiter::await_resume() {
  if (this->err) {
    throw runtime_error("failed to read from fd");
  }
  if (this->eof) {
    throw runtime_error("end of stream");
  }
}

void Buffer::ReadExactlyAwaiter::on_read_ready(evutil_socket_t fd, short what, void* ctx) {
  ReadExactlyAwaiter* aw = reinterpret_cast<ReadExactlyAwaiter*>(ctx);
  ssize_t bytes_read = evbuffer_read(
      aw->buf.buf, aw->event.get_fd(), aw->limit - aw->bytes_read);
  if (bytes_read < 0) {
    aw->err = true;
    aw->coro.resume();
    return;
  }
  aw->bytes_read += bytes_read;
  if (bytes_read == 0) {
    aw->eof = true;
    aw->coro.resume();
  } else if (aw->bytes_read != aw->limit) {
    aw->event.add();
  } else {
    aw->coro.resume();
  }
}

Buffer::WriteAwaiter::WriteAwaiter(
    Buffer& buf,
    evutil_socket_t fd,
    ssize_t limit)
  : buf(buf),
    event(this->buf.base, fd, EV_WRITE, &WriteAwaiter::on_write_ready, this),
    limit(limit),
    bytes_written(0),
    err(false),
    coro(nullptr) { }

bool Buffer::WriteAwaiter::await_ready() const noexcept {
  return false;
}

void Buffer::WriteAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event.add();
}

void Buffer::WriteAwaiter::await_resume() {
  if (this->err) {
    throw runtime_error("failed to write to fd");
  }
}

void Buffer::WriteAwaiter::on_write_ready(evutil_socket_t fd, short what, void* ctx) {
  WriteAwaiter* aw = reinterpret_cast<WriteAwaiter*>(ctx);

  if (aw->limit < 0) {
    aw->limit = aw->buf.get_length();
  }

  ssize_t bytes_written = evbuffer_write_atmost(aw->buf.buf, fd, aw->limit);
  if (bytes_written < 0) {
    aw->err = true;
    aw->coro.resume();
    return;
  }
  aw->bytes_written += bytes_written;
  if (aw->bytes_written != aw->limit) {
    aw->event.add();
  } else {
    aw->coro.resume();
  }
}

} // namespace EventAsync
