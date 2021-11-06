#pragma once

#include <event2/event.h>

#include <memory>
#include <experimental/coroutine>



class EventBase;

class Event {
public:
  Event();
  Event(
      EventBase& base,
      evutil_socket_t fd,
      short what,
      void (*cb)(evutil_socket_t fd, short what, void* ctx),
      void* ctx);
  Event(const Event& ev) = delete;
  Event(Event&& ev);
  Event& operator=(const Event& ev) = delete;
  Event& operator=(Event&& ev);
  ~Event();

  int get_fd();
  short get_what();

  virtual void add();
  void del();

protected:
  struct event* ev;
};

class TimeoutEvent : public Event {
public:
  TimeoutEvent(
      EventBase& base,
      uint64_t timeout,
      void (*cb)(evutil_socket_t fd, short what, void* ctx),
      void* ctx);
  virtual ~TimeoutEvent() = default;

  virtual void add();

protected:
  uint64_t timeout;
};

class SignalEvent : public Event {
public:
  SignalEvent(
      EventBase& base,
      int signum,
      void (*cb)(evutil_socket_t fd, short what, void* ctx),
      void* ctx);
  virtual ~SignalEvent() = default;
};



// TODO: merge this with TimeoutAwaiter somehow
class EventAwaiter {
public:
  EventAwaiter(EventBase& base, evutil_socket_t fd, short what);
  bool await_ready() const;
  void await_suspend(std::experimental::coroutine_handle<> coro);
  void await_resume();
private:
  Event event;
  std::experimental::coroutine_handle<> coro;
  static void on_trigger(evutil_socket_t fd, short what, void* ctx);
};

class TimeoutAwaiter {
public:
  TimeoutAwaiter(EventBase& base, uint64_t timeout);
  bool await_ready() const;
  void await_suspend(std::experimental::coroutine_handle<> coro);
  void await_resume();
private:
  TimeoutEvent event;
  std::experimental::coroutine_handle<> coro;
  static void on_trigger(evutil_socket_t fd, short what, void* ctx);
};
