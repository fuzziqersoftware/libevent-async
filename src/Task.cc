#include "Task.hh"

using namespace std;



namespace EventAsync {

void Task<void>::Awaiter::await_resume() const {
  // We still need to call .result() in case there's an exception to re-throw.
  // TODO: see if we can do this in a less ugly way
  reinterpret_cast<Task<void>*>(this->task)->result();
}

void Task<void>::result() const {
  this->coro.promise().result();
}



DetachedTaskCoroutine::DetachedTaskCoroutine(coroutine_handle<> coro) noexcept
  : coro(coro) { }

bool DetachedTaskCoroutine::cancel() noexcept {
  bool should_destroy = !this->destroyed.exchange(true);
  if (should_destroy) {
    this->coro.destroy();
  }
  return should_destroy;
}

DetachedTask::DetachedTask(std::shared_ptr<DetachedTaskCoroutine> handle)
  : handle(handle) { }

bool DetachedTask::cancel() noexcept {
  return this->handle->cancel();
}

DetachedTaskPromise::FinalAwaiter::FinalAwaiter(
    shared_ptr<DetachedTaskCoroutine> handle) noexcept
  : handle(handle) { }

void DetachedTaskPromise::FinalAwaiter::await_resume() const noexcept { }

bool DetachedTaskPromise::FinalAwaiter::await_ready() const noexcept {
  return false;
}

void DetachedTaskPromise::FinalAwaiter::await_suspend(
    coroutine_handle<>) noexcept {
  this->handle->cancel();
}

DetachedTaskPromise::DetachedTaskPromise()
  : handle(new DetachedTaskCoroutine(
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

} // namespace EventAsync
