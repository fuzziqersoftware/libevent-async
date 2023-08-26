#include "Event.hh"
#include "Base.hh"

#include <phosg/Time.hh>

using namespace std;

namespace EventAsync {

Event::Event() : ev(nullptr) {}

Event::Event(
    Base& base,
    evutil_socket_t fd,
    short what,
    void (*cb)(evutil_socket_t fd, short what, void* ctx),
    void* ctx)
    : ev(event_new(base.base, fd, what, cb, ctx)) {
  if (!this->ev) {
    throw bad_alloc();
  }
}

Event::Event(Event&& other) : ev(other.ev) {
  other.ev = nullptr;
}

Event& Event::operator=(Event&& other) {
  this->ev = other.ev;
  other.ev = nullptr;
  return *this;
}

Event::~Event() {
  if (this->ev) {
    event_free(this->ev);
  }
}

int Event::get_fd() {
  return event_get_fd(this->ev);
}

short Event::get_what() {
  return event_get_events(this->ev);
}

void Event::add() {
  if (event_add(this->ev, nullptr)) {
    throw runtime_error("event_add failed");
  }
}

void Event::del() {
  if (event_del(this->ev)) {
    throw runtime_error("event_del failed");
  }
}

TimeoutEvent::TimeoutEvent(
    Base& base,
    uint64_t timeout,
    void (*cb)(evutil_socket_t fd, short what, void* ctx),
    void* ctx)
    : Event(base, -1, EV_TIMEOUT, cb, ctx),
      timeout(timeout) {}

void TimeoutEvent::add() {
  auto tv = usecs_to_timeval(this->timeout);
  if (event_add(this->ev, &tv)) {
    throw runtime_error("event_add failed");
  }
}

SignalEvent::SignalEvent(
    Base& base,
    int signum,
    void (*cb)(evutil_socket_t fd, short what, void* ctx),
    void* ctx)
    : Event(base, signum, EV_SIGNAL | EV_PERSIST, cb, ctx) {}

EventAwaiter::EventAwaiter(Base& base, evutil_socket_t fd, short what)
    : event(base, fd, what, &EventAwaiter::on_trigger, this),
      coro(nullptr) {}

bool EventAwaiter::await_ready() const {
  return false;
}

void EventAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event.add();
}

void EventAwaiter::await_resume() {
  this->coro = nullptr;
}

void EventAwaiter::on_trigger(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<EventAwaiter*>(ctx)->coro.resume();
}

TimeoutAwaiter::TimeoutAwaiter(Base& base, uint64_t timeout)
    : event(base, timeout, &TimeoutAwaiter::on_trigger, this),
      coro(nullptr) {}

bool TimeoutAwaiter::await_ready() const {
  return false;
}

void TimeoutAwaiter::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->event.add();
}

void TimeoutAwaiter::await_resume() {
  this->coro = nullptr;
}

void TimeoutAwaiter::on_trigger(evutil_socket_t, short, void* ctx) {
  reinterpret_cast<TimeoutAwaiter*>(ctx)->coro.resume();
}

} // namespace EventAsync
