#pragma once

#include <event2/event.h>

#include <coroutine>
#include <string>
#include <vector>

#include "Config.hh"
#include "Event.hh"
#include "Future.hh"
#include "Task.hh"

namespace EventAsync {

class Base {
public:
  Base();
  Base(Config& config);
  Base(const Base& base) = delete;
  Base(Base&& base) = delete;
  Base& operator=(const Base& base) = delete;
  Base& operator=(Base&& base) = delete;
  ~Base();

  void run();

  void once(
      evutil_socket_t fd,
      short what,
      std::function<void(evutil_socket_t, short)> cb,
      uint64_t timeout_usecs);
  void once(
      evutil_socket_t fd,
      short what,
      void (*cb)(evutil_socket_t, short, void*),
      void* cbarg,
      uint64_t timeout_usecs);

  void call_soon(std::function<void(evutil_socket_t, short)> cb);

  // These awaiters should not be used directly; instead, you should use the
  // functions below (base.read, base.write, etc.).

  struct RecvFromResult {
    std::string data;
    size_t data_available; // may be longer than data if max_size was too small
    struct sockaddr_storage addr;
  };

  class RecvFromAwaiter {
  public:
    RecvFromAwaiter(Base& base, evutil_socket_t fd, size_t max_size);
    bool await_ready();
    void await_suspend(std::coroutine_handle<> coro);
    RecvFromResult&& await_resume();

  protected:
    static void on_read_ready(evutil_socket_t fd, short what, void* ctx);
    Base& base;
    Event event;
    evutil_socket_t fd;
    size_t max_size;
    RecvFromResult res;
    bool err;
    std::coroutine_handle<> coro;
  };

  class ReadAwaiter {
  public:
    ReadAwaiter(
        Base& base,
        evutil_socket_t fd,
        void* data,
        size_t size);
    bool await_ready();
    void await_suspend(std::coroutine_handle<> coro);
    void await_resume();

  protected:
    static void on_read_ready(evutil_socket_t fd, short what, void* ctx);
    Base& base;
    Event event;
    evutil_socket_t fd;
    void* data;
    size_t size;
    bool err;
    bool eof;
    std::coroutine_handle<> coro;
  };

  class WriteAwaiter {
  public:
    WriteAwaiter(
        Base& base,
        evutil_socket_t fd,
        const void* data,
        size_t size);
    bool await_ready();
    void await_suspend(std::coroutine_handle<> coro);
    void await_resume();

  protected:
    static void on_write_ready(evutil_socket_t fd, short what, void* ctx);
    Base& base;
    Event event;
    evutil_socket_t fd;
    const void* data;
    size_t size;
    bool err;
    std::coroutine_handle<> coro;
  };

  class AcceptAwaiter {
  public:
    AcceptAwaiter(
        Base& base, int listen_fd, struct sockaddr_storage* addr = nullptr);
    int await_resume();
    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> coro);

  private:
    static void on_accept_ready(int fd, short what, void* ctx);

    int listen_fd;
    int accepted_fd;
    Base& base;
    Event event;
    struct sockaddr_storage* addr;
    bool err;
    std::coroutine_handle<> coro;
  };

  TimeoutAwaiter sleep(uint64_t usecs);

  ReadAwaiter read(evutil_socket_t fd, void* data, size_t size);
  Task<std::string> read(evutil_socket_t fd, size_t size);

  WriteAwaiter write(evutil_socket_t fd, const void* data, size_t size);
  WriteAwaiter write(evutil_socket_t fd, const std::string& data);

  RecvFromAwaiter recvfrom(evutil_socket_t fd, size_t max_size = 1500);

  // Note: sendto() essentially never blocks so there is no async version of it

  Task<int> connect(const std::string& addr, int port);
  AcceptAwaiter accept(int listen_fd, struct sockaddr_storage* addr = nullptr);

  void dump_events(FILE* stream);

  struct event_base* base;

protected:
  static void dispatch_once_cb(evutil_socket_t fd, short what, void* ctx);
};

template <typename ValueT>
class DeferredFuture : public Future<ValueT> {
public:
  explicit DeferredFuture(Base& base) : base(base) {}
  DeferredFuture(Base& base, const ValueT& v) : Future<ValueT>(v),
                                                base(base) {}
  DeferredFuture(Base& base, ValueT&& v) : Future<ValueT>(std::move(v)),
                                           base(base) {}
  DeferredFuture(const DeferredFuture&) = delete;
  DeferredFuture(DeferredFuture&&);
  DeferredFuture& operator=(const DeferredFuture&) = delete;
  DeferredFuture& operator=(DeferredFuture&&);
  virtual ~DeferredFuture() = default;

  void resume_awaiters() {
    static auto resume_coro = +[](evutil_socket_t, short, void* addr) {
      std::coroutine_handle<>::from_address(addr).resume();
    };
    while (!this->awaiting_coros.empty()) {
      auto coro = this->awaiting_coros.front();
      this->awaiting_coros.pop_front();
      this->base.once(-1, EV_TIMEOUT, resume_coro, coro.address(), 0);
    }
  }

protected:
  Base& base;
};

} // namespace EventAsync
