#include "EventBase.hh"

#include <unistd.h>

#include <phosg/Time.hh>
#include <phosg/Network.hh>

#include "Event.hh"

using namespace std;
using namespace std::experimental;



namespace EventAsync {

EventBase::EventBase()
  : base(event_base_new()) {
  if (!this->base) {
    throw bad_alloc();
  }
}

EventBase::EventBase(EventConfig& config)
  : base(event_base_new_with_config(config.get())) {
  if (!this->base) {
    throw runtime_error("event_base_new_with_config failed");
  }
}

EventBase::~EventBase() {
  event_base_free(this->base);
}

void EventBase::run() {
  if (event_base_dispatch(this->base) < 0) {
    throw runtime_error("event_base_dispatch failed");
  }
}

void EventBase::dispatch_once_cb(evutil_socket_t fd, short what, void* ctx) {
  auto* fn = reinterpret_cast<function<void(evutil_socket_t, short)>*>(ctx);
  (*fn)(fd, what);
  delete fn;
}

void EventBase::once(
    evutil_socket_t fd,
    short what,
    function<void(evutil_socket_t, short)> cb,
    uint64_t timeout_usecs) {
  auto tv = usecs_to_timeval(timeout_usecs);
  // TODO: can we do this without an extra allocation?
  auto fn = new function<void(evutil_socket_t, short)>(cb);
  if (event_base_once(this->base, fd, what, &EventBase::dispatch_once_cb, fn,
      &tv)) {
    delete fn;
    throw runtime_error("event_base_once failed");
  }
}

void EventBase::once(
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

TimeoutAwaiter EventBase::sleep(uint64_t usecs) {
  return TimeoutAwaiter(*this, usecs);
}

EventBase::ReadAwaiter EventBase::read(evutil_socket_t fd, void* data, size_t size) {
  return ReadAwaiter(*this, fd, data, size);
}

Task<string> EventBase::read(evutil_socket_t fd, size_t size) {
  string ret(size, '\0');
  co_await this->read(fd, const_cast<char*>(ret.data()), ret.size());
  co_return ret;
}

EventBase::WriteAwaiter EventBase::write(evutil_socket_t fd, const void* data, size_t size) {
  return WriteAwaiter(*this, fd, data, size);
}

EventBase::WriteAwaiter EventBase::write(evutil_socket_t fd, const std::string& data) {
  return WriteAwaiter(*this, fd, data.data(), data.size());
}

Task<int> EventBase::connect(const std::string& addr, int port) {
  int fd = ::connect(addr, port, true);
  co_await EventAwaiter(*this, fd, EV_WRITE);
  co_return move(fd);
}



EventBase::ReadAwaiter::ReadAwaiter(
    EventBase& base,
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
    coro(nullptr) { }

bool EventBase::ReadAwaiter::await_ready() {
  ssize_t bytes_read = ::read(this->fd, this->data, this->size);
  if (bytes_read < 0) {
    // Failed to read for some reason. Try again later if there's just no data;
    // otherwise throw an exception to the awaiting coroutine.
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return false;
    } else {
      throw runtime_error("failed to read from fd");
    }

  } else if (bytes_read < this->size) {
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

void EventBase::ReadAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event = Event(
      this->base, this->fd, EV_READ, &ReadAwaiter::on_read_ready, this);
  this->event.add();
}

void EventBase::ReadAwaiter::await_resume() {
  // TODO: these should be more descriptive, and different types
  if (this->err) {
    throw runtime_error("failed to read from fd");
  }
  if (this->eof) {
    throw runtime_error("end of stream");
  }
}

void EventBase::ReadAwaiter::on_read_ready(evutil_socket_t fd, short what, void* ctx) {
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

  } else if (bytes_read < aw->size) {
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



EventBase::WriteAwaiter::WriteAwaiter(
    EventBase& base,
    evutil_socket_t fd,
    const void* data,
    size_t size)
  : base(base),
    event(),
    fd(fd),
    data(data),
    size(size),
    err(false),
    coro(nullptr) { }

bool EventBase::WriteAwaiter::await_ready() {
  ssize_t bytes_written = ::write(this->fd, this->data, this->size);
  if (bytes_written < 0) {
    // Failed to write for some reason. Try again later if the buffer is full;
    // otherwise throw an exception to the awaiting coroutine.
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return false;
    } else {
      throw runtime_error("failed to write to fd");
    }

  } else if (bytes_written < this->size) {
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

void EventBase::WriteAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event = Event(this->base, this->fd, EV_WRITE, &WriteAwaiter::on_write_ready, this);
  this->event.add();
}

void EventBase::WriteAwaiter::await_resume() {
  // TODO: this should be more descriptive
  if (this->err) {
    throw runtime_error("failed to write to fd");
  }
}

void EventBase::WriteAwaiter::on_write_ready(evutil_socket_t fd, short what, void* ctx) {
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

  } else if (bytes_written < aw->size) {
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



void EventBase::dump_events(FILE* stream) {
  event_base_dump_events(this->base, stream);
}

} // namespace EventAsync
