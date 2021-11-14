#pragma once

#include <experimental/coroutine>
#include <memory>
#include <variant>



namespace EventAsync {

template<typename ReturnT>
class TaskPromise;

template <typename ReturnT>
class Task {
public:
  using promise_type = TaskPromise<ReturnT>;

  class Awaiter {
  public:
    explicit Awaiter(std::experimental::coroutine_handle<promise_type> coro)
      : coro(coro) { }

    bool await_ready() const noexcept {
      return this->coro.done();
    }

    std::experimental::coroutine_handle<> await_suspend(
        std::experimental::coroutine_handle<> awaiting_coro) const noexcept {
      this->coro.promise().set_awaiting_coro(awaiting_coro);
      return this->coro;
    }

  protected:
    std::experimental::coroutine_handle<promise_type> coro;
  };

  class CopyAwaiter : public Awaiter {
  public:
    using Awaiter::Awaiter;
    ReturnT& await_resume() const {
      return this->coro.promise().result();
    }
  };

  class MoveAwaiter : public Awaiter {
  public:
    using Awaiter::Awaiter;
    ReturnT&& await_resume() const {
      return std::move(this->coro.promise().result());
    }
  };

  Task() noexcept = default;
  Task(std::experimental::coroutine_handle<promise_type> coro) noexcept
    : coro(coro) { }
  Task(const Task& other) = delete;
  Task(Task&& other) noexcept : coro(other.coro) {
    other.coro = nullptr;
  }
  ~Task() noexcept {
    if (this->coro != nullptr) {
      this->coro.destroy();
    }
  }
  Task& operator=(const Task& other) = delete;
  Task& operator=(Task&& other) noexcept {
    if (this->coro != nullptr) {
      this->coro.destroy();
    }
    this->coro = other.coro;
    other.coro = nullptr;
    return *this;
  }

  CopyAwaiter operator co_await() const& noexcept {
    return CopyAwaiter(this->coro);
  }

  MoveAwaiter operator co_await() const&& noexcept {
    return MoveAwaiter(this->coro);
  }

 private:
   std::experimental::coroutine_handle<promise_type> coro;
};

template <>
class Task<void> {
public:
  using promise_type = TaskPromise<void>;

  class Awaiter {
  public:
    explicit Awaiter(std::experimental::coroutine_handle<promise_type> coro)
      : coro(coro) { }

    bool await_ready() const noexcept {
      return this->coro.done();
    }

    // Note: these cannot be defined here because calling this->coro.promise()
    // requires instantiating TaskPromise, but it has not been fully defined
    // yet.
    std::experimental::coroutine_handle<> await_suspend(
        std::experimental::coroutine_handle<> awaiting_coro) const noexcept;
    void await_resume() const;

  protected:
    std::experimental::coroutine_handle<promise_type> coro;
  };

  Task() noexcept = default;
  Task(std::experimental::coroutine_handle<promise_type> coro) noexcept
    : coro(coro) { }
  Task(const Task& other) = delete;
  Task(Task&& other) noexcept : coro(other.coro) {
    other.coro = nullptr;
  }
  ~Task() noexcept {
    if (this->coro != nullptr) {
      this->coro.destroy();
    }
  }
  Task& operator=(const Task& other) = delete;
  Task& operator=(Task&& other) noexcept {
    if (this->coro != nullptr) {
      this->coro.destroy();
    }
    this->coro = other.coro;
    other.coro = nullptr;
    return *this;
  }

  Awaiter operator co_await() const noexcept {
    return Awaiter(this->coro);
  }

 private:
   std::experimental::coroutine_handle<promise_type> coro;
};



template<typename ReturnT>
class TaskPromise {
public:
  class FinalAwaiter {
  public:
    bool await_ready() const noexcept {
      return false;
    }
    void await_resume() const noexcept { }

    template <class Promise>
    std::experimental::coroutine_handle<> await_suspend(
        std::experimental::coroutine_handle<Promise> coro) noexcept {
      return coro.promise().awaiting_coro;
    }
  };

  void set_awaiting_coro(std::experimental::coroutine_handle<> coro) noexcept {
    this->awaiting_coro = coro;
  }

  ReturnT& result() {
    auto* result = std::get_if<0>(&this->value);
    if (result != nullptr) {
      return *result;
    } else {
      std::rethrow_exception(std::get<1>(this->value));
    }
  }

  Task<ReturnT> get_return_object() {
    return Task<ReturnT>(
        std::experimental::coroutine_handle<TaskPromise>::from_promise(*this));
  }

  std::experimental::suspend_always initial_suspend() noexcept {
    return std::experimental::suspend_always();
  }

  FinalAwaiter final_suspend() noexcept {
    return FinalAwaiter();
  }

  void return_value(ReturnT&& value) {
    this->value.template emplace<0>(std::forward<ReturnT>(value));
  }

  void unhandled_exception() noexcept {
    this->value.template emplace<1>(std::current_exception());
  }

private:
  std::experimental::coroutine_handle<> awaiting_coro;
  std::variant<ReturnT, std::exception_ptr> value;
};

template<>
class TaskPromise<void> {
public:
  class FinalAwaiter {
  public:
    bool await_ready() const noexcept {
      return false;
    }
    void await_resume() const noexcept { }

    template <class Promise>
    std::experimental::coroutine_handle<> await_suspend(
        std::experimental::coroutine_handle<Promise> coro) noexcept {
      return coro.promise().awaiting_coro;
    }
  };

  void set_awaiting_coro(std::experimental::coroutine_handle<> coro) noexcept {
    this->awaiting_coro = coro;
  }

  void result() {
    if (this->exc != nullptr) {
      std::rethrow_exception(this->exc);
    }
  }

  Task<void> get_return_object() {
    return Task<void>(
        std::experimental::coroutine_handle<TaskPromise>::from_promise(*this));
  }

  std::experimental::suspend_always initial_suspend() noexcept {
    return std::experimental::suspend_always();
  }

  FinalAwaiter final_suspend() noexcept {
    return FinalAwaiter();
  }

  void return_void() const noexcept { }

  void unhandled_exception() noexcept {
    this->exc = std::current_exception();
  }

private:
  std::experimental::coroutine_handle<> awaiting_coro;
  std::exception_ptr exc;
};



class DetachedTaskPromise;

class DetachedTaskCoroutine {
public:
  explicit DetachedTaskCoroutine(std::experimental::coroutine_handle<> coro) noexcept;
  // Note: this is also called by the final awaiter when the task returns
  // naturally, so it's technically misnamed... external callers will only need
  // to think of it as cancellation though.
  bool cancel() noexcept;

private:
  std::atomic<bool> destroyed;
  std::experimental::coroutine_handle<void> coro;
};

class DetachedTask {
public:
  using promise_type = DetachedTaskPromise;

  explicit DetachedTask(std::shared_ptr<DetachedTaskCoroutine> handle);
  bool cancel() noexcept;
private:
  std::shared_ptr<DetachedTaskCoroutine> handle;
};

class DetachedTaskPromise {
public:
  class FinalAwaiter {
  public:
    explicit FinalAwaiter(std::shared_ptr<DetachedTaskCoroutine> handle) noexcept;
    void await_resume() const noexcept;
    bool await_ready() const noexcept;
    void await_suspend(std::experimental::coroutine_handle<> coro) noexcept;
  private:
    std::shared_ptr<DetachedTaskCoroutine> handle;
  };

  DetachedTaskPromise();

  DetachedTask get_return_object();
  std::experimental::suspend_never initial_suspend() noexcept;
  FinalAwaiter final_suspend() noexcept;
  void return_void() const noexcept;
  void unhandled_exception() const noexcept;

private:
  std::shared_ptr<DetachedTaskCoroutine> handle;
};

} // namespace EventAsync
