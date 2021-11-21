#pragma once

#include <experimental/coroutine>
#include <memory>
#include <unordered_set>
#include <variant>



namespace EventAsync {

template<typename ReturnT>
class TaskPromise;

template <typename PromiseT>
class [[nodiscard]] TaskBase {
public:
  using promise_type = PromiseT;

  class AwaiterBase {
  public:
    explicit AwaiterBase(TaskBase* task) : task(task) { }

    bool await_ready() const noexcept {
      return this->task->coro.done();
    }

    std::experimental::coroutine_handle<> await_suspend(
        std::experimental::coroutine_handle<> awaiting_coro) const noexcept {
      this->task->link(awaiting_coro);
      if (this->task->started) {
        return std::experimental::noop_coroutine();
      } else {
        this->task->started = true;
        return this->task->coro;
      }
    }

  protected:
    TaskBase* task;
  };

  class NoReturnAwaiter : public AwaiterBase {
  public:
    using AwaiterBase::AwaiterBase;
    void await_resume() { }
  };

  class AnyAwaiter {
  public:
    AnyAwaiter() = default;

    void add_task(TaskBase* task) {
      this->tasks.emplace(task);
    }
    size_t num_tasks() const {
      return this->tasks.size();
    }

    bool await_ready() const {
      for (const auto* task : this->tasks) {
        if (task->done()) {
          return true;
        }
        if (!task->started) {
          throw std::logic_error("all tasks must be started before using AnyAwaiter");
        }
      }
      return false;
    }

    void await_suspend(std::experimental::coroutine_handle<> awaiting_coro) const {
      for (auto* task : this->tasks) {
        task->link(awaiting_coro);
      }
    }

    TaskBase* await_resume() {
      TaskBase* ret = nullptr;
      for (auto task_it = this->tasks.begin(); task_it != this->tasks.end();) {
        if ((*task_it)->done()) {
          if (ret == nullptr) {
            ret = *task_it;
          }
          task_it = this->tasks.erase(task_it);
        } else {
          (*task_it)->link(nullptr);
          task_it++;
        }
      }
      if (ret == nullptr) {
        throw std::logic_error("no task resumed during AnyAwaiter suspend");
      }
      return ret;
    }

  protected:
    std::unordered_set<TaskBase*> tasks;
  };

  TaskBase() noexcept = default;
  TaskBase(std::experimental::coroutine_handle<promise_type> coro) noexcept
    : coro(coro), started(false) { }
  TaskBase(const TaskBase& other) = delete;
  TaskBase(TaskBase&& other) noexcept : coro(other.coro), started(other.started) {
    other.coro = nullptr;
  }
  ~TaskBase() noexcept {
    if (this->coro != nullptr) {
      this->coro.destroy();
    }
  }
  TaskBase& operator=(const TaskBase& other) = delete;
  TaskBase& operator=(TaskBase&& other) noexcept {
    if (this->coro != nullptr) {
      this->coro.destroy();
    }
    this->coro = other.coro;
    this->started = other.started;
    other.coro = nullptr;
    return *this;
  }

  void start() {
    if (!this->started) {
      this->started = true;
      this->coro.resume();
    }
  }

  NoReturnAwaiter wait() {
    return NoReturnAwaiter(this);
  }

  bool done() const {
    return this->coro.done();
  }

  void link(std::experimental::coroutine_handle<> coro) {
    this->coro.promise().set_awaiting_coro(coro);
  }

protected:
  std::experimental::coroutine_handle<promise_type> coro;
  bool started;
};

template <typename ReturnT>
class [[nodiscard]] Task : public TaskBase<TaskPromise<ReturnT>> {
public:
  using TaskBase<TaskPromise<ReturnT>>::TaskBase;

  class CopyAwaiter : public TaskBase<TaskPromise<ReturnT>>::AwaiterBase {
  public:
    using TaskBase<TaskPromise<ReturnT>>::AwaiterBase::AwaiterBase;
    ReturnT& await_resume() const {
      Task<ReturnT>* t = reinterpret_cast<Task<ReturnT>*>(this->task);
      return t->result();
    }
  };

  class MoveAwaiter : public TaskBase<TaskPromise<ReturnT>>::AwaiterBase {
  public:
    using TaskBase<TaskPromise<ReturnT>>::AwaiterBase::AwaiterBase;
    ReturnT&& await_resume() const {
      Task<ReturnT>* t = reinterpret_cast<Task<ReturnT>*>(this->task);
      return std::move(t->result());
    }
  };

  CopyAwaiter operator co_await() & noexcept {
    return CopyAwaiter(this);
  }

  MoveAwaiter operator co_await() && noexcept {
    return MoveAwaiter(this);
  }

  ReturnT& result() {
    return this->coro.promise().result();
  }

  const ReturnT& result() const {
    return this->coro.promise().result();
  }
};

template <>
class [[nodiscard]] Task<void> : public TaskBase<TaskPromise<void>> {
public:
  using TaskBase<TaskPromise<void>>::TaskBase;

  class Awaiter : public TaskBase<TaskPromise<void>>::AwaiterBase {
  public:
    using TaskBase<TaskPromise<void>>::AwaiterBase::AwaiterBase;
    void await_resume() const;
  };

  Awaiter operator co_await() noexcept {
    return Awaiter(this);
  }

  void result() const;
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
      auto awaiting_coro = coro.promise().awaiting_coro;
      if (awaiting_coro) {
        return coro.promise().awaiting_coro;
      } else {
        return std::experimental::noop_coroutine();
      }
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
      auto awaiting_coro = coro.promise().awaiting_coro;
      if (awaiting_coro) {
        return coro.promise().awaiting_coro;
      } else {
        return std::experimental::noop_coroutine();
      }
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



template <typename IteratorT>
Task<void> all(IteratorT begin_it, IteratorT end_it) {
  for (IteratorT it = begin_it; it != end_it; it++) {
    it->start();
  }
  for (IteratorT it = begin_it; it != end_it; it++) {
    // Note: We ignore exceptions here because the caller is expected to call
    // .result() on each task (or co_await it, even though it is guaranteed to
    // be complete when this function returns), at which point the exceptions
    // will be re-thrown.
    co_await it->wait();
  }
}

template <typename IteratorT>
Task<typename std::iterator_traits<IteratorT>::value_type*>
any(IteratorT begin_it, IteratorT end_it) {
  using TaskT = typename std::iterator_traits<IteratorT>::value_type;
  typename TaskT::AnyAwaiter aw;

  for (IteratorT it = begin_it; it != end_it; it++) {
    it->start();
    aw.add_task(&(*it));
  }
  co_return reinterpret_cast<TaskT*>(co_await aw);
}

template <typename IteratorT>
Task<void> all_limit(IteratorT begin_it, IteratorT end_it, size_t parallelism) {
  using TaskT = typename std::iterator_traits<IteratorT>::value_type;
  typename TaskT::AnyAwaiter aw;

  IteratorT start_it = begin_it;
  while (start_it != end_it || aw.num_tasks() > 0) {
    // If there are already too many tasks running, wait for at least one of
    // them to finish. Also, if there are no more tasks to start, wait for all
    // of them to finish.
    if (aw.num_tasks() >= parallelism || start_it == end_it) {
      co_await aw;

    // Otherwise, there are not too many tasks running and there are more tasks
    // to start, so start one.
    } else {
      start_it->start();
      aw.add_task(&(*start_it));
      start_it++;
    }
  }
}

} // namespace EventAsync
