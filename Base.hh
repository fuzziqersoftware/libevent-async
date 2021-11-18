#pragma once

#include <event2/event.h>

#include <experimental/coroutine>
#include <vector>
#include <string>

#include "Task.hh"
#include "Config.hh"
#include "Event.hh"



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

  class ReadAwaiter {
  public:
    ReadAwaiter(Base& base, evutil_socket_t fd, void* data, size_t size);
    bool await_ready();
    void await_suspend(std::experimental::coroutine_handle<> coro);
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
    std::experimental::coroutine_handle<> coro;
  };

  class WriteAwaiter {
  public:
    WriteAwaiter(
        Base& base,
        evutil_socket_t fd,
        const void* data,
        size_t size);
    bool await_ready();
    void await_suspend(std::experimental::coroutine_handle<> coro);
    void await_resume();
  protected:
    static void on_write_ready(evutil_socket_t fd, short what, void* ctx);
    Base& base;
    Event event;
    evutil_socket_t fd;
    const void* data;
    size_t size;
    bool err;
    std::experimental::coroutine_handle<> coro;
  };

  class AcceptAwaiter {
  public:
    AcceptAwaiter(
        Base& base, int listen_fd, struct sockaddr_storage* addr = nullptr);
    int await_resume();
    bool await_ready() const noexcept;
    void await_suspend(std::experimental::coroutine_handle<> coro);
  private:
    static void on_accept_ready(int fd, short what, void* ctx);

    int listen_fd;
    int accepted_fd;
    Base& base;
    Event event;
    struct sockaddr_storage* addr;
    bool err;
    std::experimental::coroutine_handle<> coro;
  };

  TimeoutAwaiter sleep(uint64_t usecs);

  ReadAwaiter read(evutil_socket_t fd, void* data, size_t size);
  Task<std::string> read(evutil_socket_t fd, size_t size);

  WriteAwaiter write(evutil_socket_t fd, const void* data, size_t size);
  WriteAwaiter write(evutil_socket_t fd, const std::string& data);

  Task<int> connect(const std::string& addr, int port);
  AcceptAwaiter accept(int listen_fd, struct sockaddr_storage* addr = nullptr);

  void dump_events(FILE* stream);

  struct event_base* base;

protected:
  static void dispatch_once_cb(evutil_socket_t fd, short what, void* ctx);
};

} // namespace EventAsync
