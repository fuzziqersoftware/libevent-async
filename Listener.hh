#pragma once

#include <event2/event.h>
#include <event2/listener.h>

#include <memory>
#include <phosg/Filesystem.hh>

#include "EventBase.hh"
#include "Event.hh"



namespace EventAsync {

class Listener {
public:
  Listener(EventBase& base, int fd);
  Listener(const Listener& lev) = delete;
  Listener(Listener&& lev) = default;
  Listener& operator=(const Listener& lev) = delete;
  Listener& operator=(Listener&& lev) = delete;
  ~Listener() = default;

  class Awaiter {
  public:
    Awaiter(EventBase& base, int listen_fd);
    int await_resume();
    bool await_ready() const noexcept;
    void await_suspend(std::experimental::coroutine_handle<> coro);
  private:
    static void on_accept_ready(int fd, short what, void* ctx);

    int listen_fd;
    int accepted_fd;
    EventBase& base;
    Event event;
    bool err;
    std::experimental::coroutine_handle<> coro;
  };

  Awaiter accept() const;

protected:
  EventBase& base;
  scoped_fd listen_fd;
};

} // namespace EventAsync
