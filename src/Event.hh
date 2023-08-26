#pragma once

#include <event2/event.h>

#include <memory>
#include <coroutine>



namespace EventAsync {

class Base;

class Event {
public:
  Event();
  Event(
      Base& base,
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
      Base& base,
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
      Base& base,
      int signum,
      void (*cb)(evutil_socket_t fd, short what, void* ctx),
      void* ctx);
  virtual ~SignalEvent() = default;
};



// TODO: merge this with TimeoutAwaiter somehow
class EventAwaiter {
public:
  EventAwaiter(Base& base, evutil_socket_t fd, short what);
  bool await_ready() const;
  void await_suspend(std::coroutine_handle<> coro);
  void await_resume();
private:
  Event event;
  std::coroutine_handle<> coro;
  static void on_trigger(evutil_socket_t fd, short what, void* ctx);
};

class TimeoutAwaiter {
public:
  TimeoutAwaiter(Base& base, uint64_t timeout);
  bool await_ready() const;
  void await_suspend(std::coroutine_handle<> coro);
  void await_resume();
private:
  TimeoutEvent event;
  std::coroutine_handle<> coro;
  static void on_trigger(evutil_socket_t fd, short what, void* ctx);
};

} // namespace EventAsync
