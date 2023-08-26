#include "Base.hh"

#include <unistd.h>

#include <phosg/Network.hh>
#include <phosg/Time.hh>

#include "Event.hh"
#include "Future.hh"

using namespace std;

namespace EventAsync {

Base::Base()
    : base(event_base_new()) {
  if (!this->base) {
    throw bad_alloc();
  }
}

Base::Base(Config& config)
    : base(event_base_new_with_config(config.get())) {
  if (!this->base) {
    throw runtime_error("event_base_new_with_config failed");
  }
}

Base::~Base() {
  event_base_free(this->base);
}

void Base::run() {
  if (event_base_dispatch(this->base) < 0) {
    throw runtime_error("event_base_dispatch failed");
  }
}

void Base::dispatch_once_cb(evutil_socket_t fd, short what, void* ctx) {
  auto* fn = reinterpret_cast<function<void(evutil_socket_t, short)>*>(ctx);
  (*fn)(fd, what);
  delete fn;
}

void Base::once(
    evutil_socket_t fd,
    short what,
    function<void(evutil_socket_t, short)> cb,
    uint64_t timeout_usecs) {
  auto tv = usecs_to_timeval(timeout_usecs);
  // TODO: can we do this without an extra allocation?
  auto fn = new function<void(evutil_socket_t, short)>(cb);
  if (event_base_once(this->base, fd, what, &Base::dispatch_once_cb, fn,
          &tv)) {
    delete fn;
    throw runtime_error("event_base_once failed");
  }
}

void Base::once(
    evutil_socket_t fd,
    short what,
    void (*cb)(evutil_socket_t, short, void*),
    void* cbarg,
    uint64_t timeout_usecs) {
  auto tv = usecs_to_timeval(timeout_usecs);
  if (event_base_once(this->base, fd, what, cb, cbarg, &tv)) {
    throw runtime_error("event_base_once failed");
  }
}

TimeoutAwaiter Base::sleep(uint64_t usecs) {
  return TimeoutAwaiter(*this, usecs);
}

Base::ReadAwaiter Base::read(evutil_socket_t fd, void* data, size_t size) {
  return ReadAwaiter(*this, fd, data, size);
}

Task<string> Base::read(evutil_socket_t fd, size_t size) {
  string ret(size, '\0');
  co_await this->read(fd, const_cast<char*>(ret.data()), ret.size());
  co_return std::move(ret);
}

Base::WriteAwaiter Base::write(evutil_socket_t fd, const void* data, size_t size) {
  return WriteAwaiter(*this, fd, data, size);
}

Base::WriteAwaiter Base::write(evutil_socket_t fd, const string& data) {
  return WriteAwaiter(*this, fd, data.data(), data.size());
}

Base::RecvFromAwaiter Base::recvfrom(evutil_socket_t fd, size_t max_size) {
  return RecvFromAwaiter(*this, fd, max_size);
}

Task<int> Base::connect(const std::string& addr, int port) {
  // TODO: this does a blocking DNS query if addr isn't an IP address string
  int fd = ::connect(addr, port, true);
  co_await EventAwaiter(*this, fd, EV_WRITE);
  co_return std::move(fd);
}

Base::RecvFromAwaiter::RecvFromAwaiter(
    Base& base, evutil_socket_t fd, size_t max_size)
    : base(base),
      event(),
      fd(fd),
      max_size(max_size),
      err(false),
      coro(nullptr) {}

bool Base::RecvFromAwaiter::await_ready() {
  socklen_t ss_len = sizeof(struct sockaddr_storage);
  this->res.data.resize(this->max_size, '\0');
  ssize_t bytes_read = ::recvfrom(
      this->fd, this->res.data.data(), this->res.data.size(), 0,
      reinterpret_cast<struct sockaddr*>(&this->res.addr), &ss_len);

  if (bytes_read < 0) {
    // Failed to read for some reason. Try again later if there's just no data;
    // otherwise throw an exception to the awaiting coroutine.
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return false;
    } else {
      throw runtime_error("failed to read from fd");
    }

  } else if (bytes_read == 0) {
    throw runtime_error("zero-length message");

  } else {
    // A message was read; the awaiting coroutine does not need to be suspended.
    this->res.data_available = bytes_read;
    if (this->res.data_available < this->res.data.size()) {
      this->res.data.resize(bytes_read);
    }
    return true;
  }
}

void Base::RecvFromAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event = Event(
      this->base, this->fd, EV_READ, &RecvFromAwaiter::on_read_ready, this);
  this->event.add();
}

Base::RecvFromResult&& Base::RecvFromAwaiter::await_resume() {
  if (this->err) {
    throw runtime_error("failed to read from fd");
  }
  return std::move(this->res);
}

void Base::RecvFromAwaiter::on_read_ready(evutil_socket_t, short, void* ctx) {
  RecvFromAwaiter* aw = reinterpret_cast<RecvFromAwaiter*>(ctx);

  socklen_t ss_len = sizeof(struct sockaddr_storage);
  aw->res.data.resize(aw->max_size, '\0');
  ssize_t bytes_read = ::recvfrom(
      aw->fd, aw->res.data.data(), aw->res.data.size(), 0,
      reinterpret_cast<struct sockaddr*>(&aw->res.addr), &ss_len);

  if (bytes_read < 0) {
    // Failed to read for some reason. Try again later if there's just no data;
    // otherwise throw an exception to the awaiting coroutine.
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      aw->event.add();
    } else {
      aw->err = true;
      aw->coro.resume();
    }

  } else if (bytes_read == 0) {
    aw->err = true;

  } else {
    // A message was read; the awaiting coroutine does not need to be suspended.
    aw->res.data_available = bytes_read;
    if (aw->res.data_available < aw->res.data.size()) {
      aw->res.data.resize(bytes_read);
    }
    aw->coro.resume();
  }
}

Base::ReadAwaiter::ReadAwaiter(
    Base& base,
    evutil_socket_t fd,
    void* data,
    size_t size)
    : base(base),
      event(),
      fd(fd),
      data(data),
      size(size),
      err(false),
      eof(false),
      coro(nullptr) {}

bool Base::ReadAwaiter::await_ready() {
  ssize_t bytes_read = ::read(this->fd, this->data, this->size);

  if (bytes_read < 0) {
    // Failed to read for some reason. Try again later if there's just no data;
    // otherwise throw an exception to the awaiting coroutine.
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return false;
    } else {
      throw runtime_error("failed to read from fd");
    }

  } else if (static_cast<size_t>(bytes_read) < this->size) {
    // There's more data to read. Adjust data/size and wait for more data.
    this->data = reinterpret_cast<uint8_t*>(this->data) + bytes_read;
    this->size -= bytes_read;
    return false;

  } else {
    // All requested data has been read; the awaiting coroutine does not need to
    // be suspended.
    this->size = 0;
    return true;
  }
}

void Base::ReadAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event = Event(
      this->base, this->fd, EV_READ, &ReadAwaiter::on_read_ready, this);
  this->event.add();
}

void Base::ReadAwaiter::await_resume() {
  // TODO: these should be more descriptive, and different types
  if (this->err) {
    throw runtime_error("failed to read from fd");
  }
  if (this->eof) {
    throw runtime_error("end of stream");
  }
}

void Base::ReadAwaiter::on_read_ready(evutil_socket_t, short, void* ctx) {
  ReadAwaiter* aw = reinterpret_cast<ReadAwaiter*>(ctx);

  ssize_t bytes_read = ::read(aw->fd, aw->data, aw->size);

  if (bytes_read < 0) {
    // Failed to read for some reason. Try again later if there's just no data;
    // otherwise throw an exception to the awaiting coroutine.
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      aw->event.add();
    } else {
      aw->err = true;
      aw->coro.resume();
    }

  } else if (static_cast<size_t>(bytes_read) < aw->size) {
    // There's more data to read. Adjust data/size and wait for more data.
    aw->data = reinterpret_cast<uint8_t*>(aw->data) + bytes_read;
    aw->size -= bytes_read;
    aw->event.add();

  } else {
    // All requested data has been read; the awaiting coroutine can resume.
    aw->size = 0;
    aw->coro.resume();
  }
}

Base::WriteAwaiter::WriteAwaiter(
    Base& base,
    evutil_socket_t fd,
    const void* data,
    size_t size)
    : base(base),
      event(),
      fd(fd),
      data(data),
      size(size),
      err(false),
      coro(nullptr) {}

bool Base::WriteAwaiter::await_ready() {
  ssize_t bytes_written = ::write(this->fd, this->data, this->size);
  if (bytes_written < 0) {
    // Failed to write for some reason. Try again later if the buffer is full;
    // otherwise throw an exception to the awaiting coroutine.
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return false;
    } else {
      throw runtime_error("failed to write to fd");
    }

  } else if (static_cast<size_t>(bytes_written) < this->size) {
    // There's more data to write. Adjust data/size and wait for the buffer to
    // drain.
    this->data = reinterpret_cast<const uint8_t*>(this->data) + bytes_written;
    this->size -= bytes_written;
    return false;

  } else {
    // All requested data has been written; the awaiting coroutine does not need
    // to be suspended.
    this->size = 0;
    return true;
  }
}

void Base::WriteAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event = Event(this->base, this->fd, EV_WRITE, &WriteAwaiter::on_write_ready, this);
  this->event.add();
}

void Base::WriteAwaiter::await_resume() {
  // TODO: this should be more descriptive
  if (this->err) {
    throw runtime_error("failed to write to fd");
  }
}

void Base::WriteAwaiter::on_write_ready(evutil_socket_t, short, void* ctx) {
  WriteAwaiter* aw = reinterpret_cast<WriteAwaiter*>(ctx);
  ssize_t bytes_written = ::write(aw->fd, aw->data, aw->size);
  if (bytes_written < 0) {
    // Failed to write for some reason. Try again later if the buffer is full;
    // otherwise throw an exception to the awaiting coroutine.
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      aw->event.add();
    } else {
      aw->err = true;
      aw->coro.resume();
    }

  } else if (static_cast<size_t>(bytes_written) < aw->size) {
    // There's more data to write. Adjust data/size and wait for the buffer to
    // drain.
    aw->data = reinterpret_cast<const uint8_t*>(aw->data) + bytes_written;
    aw->size -= bytes_written;
    aw->event.add();

  } else {
    // All requested data has been written; the awaiting coroutine can resume.
    aw->size = 0;
    aw->coro.resume();
  }
}

Base::AcceptAwaiter Base::accept(
    int listen_fd, struct sockaddr_storage* addr) {
  return AcceptAwaiter(*this, listen_fd, addr);
}

Base::AcceptAwaiter::AcceptAwaiter(
    Base& base,
    int listen_fd,
    struct sockaddr_storage* addr)
    : listen_fd(listen_fd),
      accepted_fd(-1),
      base(base),
      event(this->base, this->listen_fd, EV_READ, &AcceptAwaiter::on_accept_ready, this),
      addr(addr),
      err(false),
      coro(nullptr) {}

bool Base::AcceptAwaiter::await_ready() const noexcept {
  return false;
}

void Base::AcceptAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event.add();
}

int Base::AcceptAwaiter::await_resume() {
  if (this->err) {
    throw runtime_error("accept() failed");
  }
  return this->accepted_fd;
}

void Base::AcceptAwaiter::on_accept_ready(int, short, void* ctx) {
  AcceptAwaiter* aw = reinterpret_cast<AcceptAwaiter*>(ctx);
  socklen_t addr_size = aw->addr ? sizeof(*aw->addr) : 0;
  aw->accepted_fd = ::accept(
      aw->listen_fd,
      reinterpret_cast<struct sockaddr*>(aw->addr),
      aw->addr ? &addr_size : nullptr);
  if (aw->accepted_fd < 0) {
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
      aw->err = true;
    } else {
      aw->event.add(); // Try again
    }
  } else {
    aw->coro.resume(); // Got an fd; coro can resume
  }
}

void Base::dump_events(FILE* stream) {
  event_base_dump_events(this->base, stream);
}

} // namespace EventAsync
