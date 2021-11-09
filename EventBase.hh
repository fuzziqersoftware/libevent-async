#pragma once

#include <event2/event.h>

#include <experimental/coroutine>
#include <vector>
#include <string>

#include "AsyncTask.hh"
#include "EventConfig.hh"
#include "Event.hh"



class EventBase {
public:
  EventBase();
  EventBase(EventConfig& config);
  EventBase(const EventBase& base) = delete;
  EventBase(EventBase&& base) = delete;
  EventBase& operator=(const EventBase& base) = delete;
  EventBase& operator=(EventBase&& base) = delete;
  ~EventBase();

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
    ReadAwaiter(EventBase& base, evutil_socket_t fd, void* data, size_t size);
    bool await_ready();
    void await_suspend(std::experimental::coroutine_handle<> coro);
    void await_resume();
  protected:
    static void on_read_ready(evutil_socket_t fd, short what, void* ctx);
    EventBase& base;
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
        EventBase& base,
        evutil_socket_t fd,
        const void* data,
        size_t size);
    bool await_ready();
    void await_suspend(std::experimental::coroutine_handle<> coro);
    void await_resume();
  protected:
    static void on_write_ready(evutil_socket_t fd, short what, void* ctx);
    EventBase& base;
    Event event;
    evutil_socket_t fd;
    const void* data;
    size_t size;
    bool err;
    std::experimental::coroutine_handle<> coro;
  };

  TimeoutAwaiter sleep(uint64_t usecs);

  ReadAwaiter read(evutil_socket_t fd, void* data, size_t size);
  AsyncTask<std::string> read(evutil_socket_t fd, size_t size);

  WriteAwaiter write(evutil_socket_t fd, const void* data, size_t size);
  WriteAwaiter write(evutil_socket_t fd, const std::string& data);

  AsyncTask<int> connect(const std::string& addr, int port);

  void dump_events(FILE* stream);

  struct event_base* base;

protected:
  static void dispatch_once_cb(evutil_socket_t fd, short what, void* ctx);
};
