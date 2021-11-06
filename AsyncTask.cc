#include "AsyncTask.hh"

using namespace std;
using namespace std::experimental;



coroutine_handle<> AsyncTask<void>::Awaiter::await_suspend(
    coroutine_handle<> awaiting_coro) const noexcept {
  this->coro.promise().set_awaiting_coro(awaiting_coro);
  return this->coro;
}

void AsyncTask<void>::Awaiter::await_resume() const {
  // We still need to call .result() in case there's an exception to re-throw.
  this->coro.promise().result();
}



DetachedTaskHandle::DetachedTaskHandle(coroutine_handle<> coro) noexcept
  : coro(coro) { }

bool DetachedTaskHandle::cancel() noexcept {
  bool should_destroy = !this->destroyed.exchange(true);
  if (should_destroy) {
    this->coro.destroy();
  }
  return should_destroy;
}

DetachedTask::DetachedTask(std::shared_ptr<DetachedTaskHandle> handle)
  : handle(handle) { }

bool DetachedTask::cancel() noexcept {
  return this->handle->cancel();
}

DetachedTaskPromise::FinalAwaiter::FinalAwaiter(
    shared_ptr<DetachedTaskHandle> handle) noexcept
  : handle(handle) { }

void DetachedTaskPromise::FinalAwaiter::await_resume() const noexcept { }

bool DetachedTaskPromise::FinalAwaiter::await_ready() const noexcept {
  return false;
}

void DetachedTaskPromise::FinalAwaiter::await_suspend(
    coroutine_handle<> coro) noexcept {
  this->handle->cancel();
}

DetachedTaskPromise::DetachedTaskPromise()
  : handle(new DetachedTaskHandle(
      coroutine_handle<DetachedTaskPromise>::from_promise(*this))) { }

DetachedTask DetachedTaskPromise::get_return_object() {
  return DetachedTask(this->handle);
}

suspend_never DetachedTaskPromise::initial_suspend() noexcept {
  return suspend_never();
}

DetachedTaskPromise::FinalAwaiter DetachedTaskPromise::final_suspend() noexcept {
  return FinalAwaiter(this->handle);
}

void DetachedTaskPromise::return_void() const noexcept { }

void DetachedTaskPromise::unhandled_exception() const noexcept {
  terminate();
}
