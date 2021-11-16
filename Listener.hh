#pragma once

#include <event2/event.h>
#include <event2/listener.h>

#include <memory>
#include <phosg/Filesystem.hh>

#include "Base.hh"
#include "Event.hh"



namespace EventAsync {

class Listener {
public:
  Listener(Base& base, int fd);
  Listener(const Listener& lev) = delete;
  Listener(Listener&& lev) = default;
  Listener& operator=(const Listener& lev) = delete;
  Listener& operator=(Listener&& lev) = delete;
  ~Listener() = default;

  class Awaiter {
  public:
    Awaiter(Base& base, int listen_fd);
    int await_resume();
    bool await_ready() const noexcept;
    void await_suspend(std::experimental::coroutine_handle<> coro);
  private:
    static void on_accept_ready(int fd, short what, void* ctx);

    int listen_fd;
    int accepted_fd;
    Base& base;
    Event event;
    bool err;
    std::experimental::coroutine_handle<> coro;
  };

  Awaiter accept() const;

protected:
  Base& base;
  scoped_fd listen_fd;
};

} // namespace EventAsync
