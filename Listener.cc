#include "Listener.hh"

using namespace std;



Listener::Listener(EventBase& base, int fd) : base(base), listen_fd(fd) { }

Listener::Awaiter Listener::accept() const {
  return Awaiter(this->base, this->listen_fd);
}

Listener::Awaiter::Awaiter(EventBase& base, int listen_fd)
  : listen_fd(listen_fd),
    accepted_fd(-1),
    base(base),
    event(this->base, this->listen_fd, EV_READ, &Awaiter::on_accept_ready, this),
    err(false),
    coro(nullptr) { }

bool Listener::Awaiter::await_ready() const noexcept {
  return false;
}

void Listener::Awaiter::await_suspend(std::experimental::coroutine_handle<> coro) {
  this->coro = coro;
  this->event.add();
}

int Listener::Awaiter::await_resume() {
  if (this->err) {
    throw runtime_error("accept() failed");
  }
  return this->accepted_fd;
}

void Listener::Awaiter::on_accept_ready(int fd, short what, void* ctx) {
  Awaiter* aw = reinterpret_cast<Awaiter*>(ctx);
  aw->accepted_fd = ::accept(aw->listen_fd, nullptr, nullptr);
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
