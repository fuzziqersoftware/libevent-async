#include "EvBuffer.hh"

#include <phosg/Strings.hh>

using namespace std;
using namespace std::experimental;



EvBuffer::EvBuffer(EventBase& base)
  : base(base), buf(evbuffer_new()), owned(true) {
  if (!this->buf) {
    throw runtime_error("evbuffer_new");
  }
}

EvBuffer::EvBuffer(EventBase& base, struct evbuffer* buf)
  : base(base), buf(buf), owned(false) { }

EvBuffer::EvBuffer(EvBuffer&& other) : base(other.base), buf(other.buf) {
  other.buf = nullptr;
  other.owned = false;
}

EvBuffer::~EvBuffer() {
  if (this->owned) {
    evbuffer_free(this->buf);
  }
}

void EvBuffer::enable_locking(void* lock) {
  if (evbuffer_enable_locking(this->buf, lock)) {
    throw runtime_error("evbuffer_enable_locking");
  }
}

void EvBuffer::lock() {
  evbuffer_lock(this->buf);
}

void EvBuffer::unlock() {
  evbuffer_unlock(this->buf);
}

size_t EvBuffer::get_length() const {
  return evbuffer_get_length(this->buf);
}

size_t EvBuffer::get_contiguous_space() const {
  return evbuffer_get_contiguous_space(this->buf);
}

void EvBuffer::expand(size_t size) {
  if (evbuffer_expand(this->buf, size)) {
    throw runtime_error("evbuffer_expand");
  }
}

void EvBuffer::add(const void* data, size_t size) {
  if (evbuffer_add(this->buf, data, size)) {
    throw runtime_error("evbuffer_add");
  }
}

size_t EvBuffer::add_printf(const char* fmt, ...) {
  va_list va;
  va_start(va, fmt);
  int ret = evbuffer_add_vprintf(this->buf, fmt, va);
  va_end(va);
  if (ret < 0) {
    throw runtime_error("evbuffer_add_vprintf");
  }
  return ret;
}

size_t EvBuffer::add_vprintf(const char* fmt, va_list va) {
  int ret = evbuffer_add_vprintf(this->buf, fmt, va);
  if (ret < 0) {
    throw runtime_error("evbuffer_add_vprintf");
  }
  return ret;
}

void EvBuffer::add_buffer(struct evbuffer* src) {
  if (evbuffer_add_buffer(this->buf, src)) {
    throw runtime_error("evbuffer_add_buffer");
  }
}

void EvBuffer::add_buffer(EvBuffer& src) {
  if (evbuffer_add_buffer(this->buf, src.buf)) {
    throw runtime_error("evbuffer_add_buffer");
  }
}

size_t EvBuffer::remove_buffer(struct evbuffer* src, size_t size) {
  int ret = evbuffer_remove_buffer(this->buf, src, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove_buffer");
  }
  return ret;
}

size_t EvBuffer::remove_buffer(EvBuffer& src, size_t size) {
  int ret = evbuffer_remove_buffer(this->buf, src.buf, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove_buffer");
  }
  return ret;
}

void EvBuffer::prepend(const void* data, size_t size) {
  if (evbuffer_prepend(this->buf, data, size)) {
    throw runtime_error("evbuffer_prepend");
  }
}

void EvBuffer::prepend_buffer(struct evbuffer* src) {
  if (evbuffer_prepend_buffer(this->buf, src)) {
    throw runtime_error("evbuffer_prepend_buffer");
  }
}

void EvBuffer::prepend_buffer(EvBuffer& src) {
  if (evbuffer_prepend_buffer(this->buf, src.buf)) {
    throw runtime_error("evbuffer_prepend_buffer");
  }
}

uint8_t* EvBuffer::pullup(ssize_t size) {
  uint8_t* ret = evbuffer_pullup(this->buf, size);
  if (!ret) {
    throw runtime_error("evbuffer_pullup");
  }
  return ret;
}

void EvBuffer::drain(size_t size) {
  if (evbuffer_drain(this->buf, size)) {
    throw runtime_error("evbuffer_drain");
  }
}

void EvBuffer::drain_all() {
  this->drain(this->get_length());
}

size_t EvBuffer::remove(void* data, size_t size) {
  int ret = evbuffer_remove(this->buf, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_remove");
  }
  return ret;
}

string EvBuffer::remove(size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  int bytes_read = evbuffer_remove(this->buf, const_cast<char*>(data.data()), size);
  if (bytes_read < 0) {
    throw runtime_error("evbuffer_remove");
  }
  data.resize(bytes_read);
  return data;
}

size_t EvBuffer::copyout(void* data, size_t size) {
  ssize_t ret = evbuffer_copyout(this->buf, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_copyout");
  }
  return ret;
}

string EvBuffer::copyout(size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  ssize_t bytes_read = evbuffer_copyout(this->buf, const_cast<char*>(data.data()), size);
  if (bytes_read < 0) {
    throw runtime_error("evbuffer_copyout");
  }
  data.resize(bytes_read);
  return data;
}

size_t EvBuffer::copyout_from(const struct evbuffer_ptr* pos, void* data, size_t size) {
  ssize_t ret = evbuffer_copyout_from(this->buf, pos, data, size);
  if (ret < 0) {
    throw runtime_error("evbuffer_copyout_from");
  }
  return ret;
}

string EvBuffer::copyout_from(const struct evbuffer_ptr* pos, size_t size) {
  // TODO: eliminate this unnecessary initialization
  string data(size, '\0');
  ssize_t bytes_read = evbuffer_copyout_from(this->buf, pos, const_cast<char*>(data.data()), size);
  if (bytes_read < 0) {
    throw runtime_error("evbuffer_copyout_from");
  }
  data.resize(bytes_read);
  return data;
}

unique_ptr<char, void(*)(void*)> EvBuffer::readln(size_t* bytes_read, enum evbuffer_eol_style eol_style) {
  char* ret = evbuffer_readln(this->buf, bytes_read, eol_style);
  if (!ret) {
    return unique_ptr<char, void(*)(void*)>(ret, free);
  } else {
    // Note: we don't throw here because not having a complete line to return is
    // a fairly common case.
    // TODO: returning unique_ptr(NULL, NULL) here might crash; fix it if so
    return unique_ptr<char, void(*)(void*)>(nullptr, nullptr);
  }
}

struct evbuffer_ptr EvBuffer::search(const char* what, size_t size,
    const struct evbuffer_ptr* start) {
  return evbuffer_search(this->buf, what, size, start);
}
struct evbuffer_ptr EvBuffer::search_range(const char* what, size_t size,
    const struct evbuffer_ptr *start, const struct evbuffer_ptr *end) {
  return evbuffer_search_range(this->buf, what, size, start, end);
}
struct evbuffer_ptr EvBuffer::search_eol(struct evbuffer_ptr* start,
    size_t* bytes_found, enum evbuffer_eol_style eol_style) {
  return evbuffer_search_eol(this->buf, start, bytes_found, eol_style);
}

void EvBuffer::ptr_set(struct evbuffer_ptr* pos, size_t position,
    enum evbuffer_ptr_how how) {
  if (evbuffer_ptr_set(this->buf, pos, position, how)) {
    throw runtime_error("evbuffer_ptr_set");
  }
}

int EvBuffer::peek(ssize_t size, struct evbuffer_ptr* start_at,
    struct evbuffer_iovec* vec_out, int n_vec) {
  return evbuffer_peek(this->buf, size, start_at, vec_out, n_vec);
}

int EvBuffer::reserve_space(ev_ssize_t size, struct evbuffer_iovec* vec, int n_vecs) {
  return evbuffer_reserve_space(this->buf, size, vec, n_vecs);
}

void EvBuffer::commit_space(struct evbuffer_iovec* vec, int n_vecs) {
  if (evbuffer_commit_space(this->buf, vec, n_vecs)) {
    throw runtime_error("evbuffer_commit_space");
  }
}

void EvBuffer::add_reference(const void* data, size_t size,
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

void EvBuffer::add_reference(const void* data, size_t size,
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
  string* s = reinterpret_cast<string*>(ctx);
  delete s;
}

void EvBuffer::add(string&& data) {
  string* s = new string(move(data));
  int ret = evbuffer_add_reference(this->buf, s->data(), s->size(),
      dispatch_delete_string, s);
  if (ret < 0) {
    data = move(*s);
    delete s;
    throw runtime_error("evbuffer_add_reference");
  }
}

void EvBuffer::add_file(int fd, off_t offset, size_t size) {
  if (evbuffer_add_file(this->buf, fd, offset, size)) {
    throw runtime_error("evbuffer_add_file");
  }
}

void EvBuffer::add_buffer_reference(struct evbuffer* other_buf) {
  if (evbuffer_add_buffer_reference(this->buf, other_buf)) {
    throw runtime_error("evbuffer_add_buffer_reference");
  }
}

void EvBuffer::add_buffer_reference(EvBuffer& other_buf) {
  if (evbuffer_add_buffer_reference(this->buf, other_buf.buf)) {
    throw runtime_error("evbuffer_add_buffer_reference");
  }
}

void EvBuffer::freeze(int at_front) {
  if (evbuffer_freeze(this->buf, at_front)) {
    throw runtime_error("evbuffer_freeze");
  }
}

void EvBuffer::unfreeze(int at_front) {
  if (evbuffer_unfreeze(this->buf, at_front)) {
    throw runtime_error("evbuffer_unfreeze");
  }
}

void EvBuffer::debug_print_contents(FILE* stream) {
  size_t size = this->get_length();
  if (size) {
    print_data(stream, this->copyout(size));
  }
}

EvBuffer::ReadAtMostAwaiter EvBuffer::read(evutil_socket_t fd, ssize_t size) {
  return ReadAtMostAwaiter(*this, fd, size);
}

EvBuffer::ReadExactlyAwaiter EvBuffer::read_exactly(
    evutil_socket_t fd, size_t size) {
  return ReadExactlyAwaiter(*this, fd, static_cast<ssize_t>(size));
}

EvBuffer::WriteAwaiter EvBuffer::write(evutil_socket_t fd, ssize_t size) {
  return WriteAwaiter(*this, fd, size);
}



EvBuffer::ReadAwaiter::ReadAwaiter(
    EvBuffer& buf,
    evutil_socket_t fd,
    ssize_t limit,
    void (*cb)(evutil_socket_t, short, void*))
  : buf(buf),
    event(this->buf.base, fd, EV_READ, cb, this),
    limit(limit),
    bytes_read(0),
    err(false),
    coro(nullptr) { }

bool EvBuffer::ReadAwaiter::await_ready() const noexcept {
  return false;
}

void EvBuffer::ReadAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event.add();
}

EvBuffer::ReadAtMostAwaiter::ReadAtMostAwaiter(
    EvBuffer& buf,
    evutil_socket_t fd,
    ssize_t limit)
  : ReadAwaiter(buf, fd, limit, &ReadAtMostAwaiter::on_read_ready) { }

size_t EvBuffer::ReadAtMostAwaiter::await_resume() {
  if (this->err) {
    throw runtime_error("failed to read from fd");
  }
  return this->bytes_read;
}

void EvBuffer::ReadAtMostAwaiter::on_read_ready(evutil_socket_t fd, short what, void* ctx) {
  ReadAtMostAwaiter* aw = reinterpret_cast<ReadAtMostAwaiter*>(ctx);
  ssize_t bytes_read = evbuffer_read(aw->buf.buf, aw->event.get_fd(), aw->limit);
  aw->err = (bytes_read < 0);
  aw->bytes_read = bytes_read;
  aw->coro.resume();
}

EvBuffer::ReadExactlyAwaiter::ReadExactlyAwaiter(
    EvBuffer& buf,
    evutil_socket_t fd,
    size_t limit)
  : ReadAwaiter(buf, fd, limit, &ReadExactlyAwaiter::on_read_ready),
    eof(false) { }

void EvBuffer::ReadExactlyAwaiter::await_resume() {
  if (this->err) {
    throw runtime_error("failed to read from fd");
  }
  if (this->eof) {
    throw runtime_error("end of stream");
  }
}

void EvBuffer::ReadExactlyAwaiter::on_read_ready(evutil_socket_t fd, short what, void* ctx) {
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

EvBuffer::WriteAwaiter::WriteAwaiter(
    EvBuffer& buf,
    evutil_socket_t fd,
    size_t limit)
  : buf(buf),
    event(this->buf.base, fd, EV_WRITE, &WriteAwaiter::on_write_ready, this),
    limit(limit),
    bytes_written(0),
    err(false),
    coro(nullptr) { }

bool EvBuffer::WriteAwaiter::await_ready() const noexcept {
  return false;
}

void EvBuffer::WriteAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event.add();
}

void EvBuffer::WriteAwaiter::await_resume() {
  if (this->err) {
    throw runtime_error("failed to write to fd");
  }
}

void EvBuffer::WriteAwaiter::on_write_ready(evutil_socket_t fd, short what, void* ctx) {
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