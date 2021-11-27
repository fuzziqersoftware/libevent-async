#pragma once

#include <experimental/coroutine>
#include <memory>
#include <unordered_set>
#include <variant>
#include <forward_list>



namespace EventAsync {

template <typename ResultT>
class FutureBase {
public:
  FutureBase() : awaiting_coros_insert_it(this->awaiting_coros.before_begin()) { }
  FutureBase(const ResultT& value) : value(value) { }
  FutureBase(ResultT&& value) : value(move(value)) { }
  FutureBase(const FutureBase&) = delete;
  FutureBase(FutureBase&&);
  FutureBase& operator=(const FutureBase&) = delete;
  FutureBase& operator=(FutureBase&&);
  virtual ~FutureBase() noexcept(false) {
    // Throwing in a destructor is a Bad Idea. But it's also a Bad Idea to let
    // a Future be destroyed while someone is waiting on it! The caller should
    // call .resume_awaiters first.
    if (!this->awaiting_coros.empty()) {
      throw std::logic_error("Future destroyed with awaiters present");
    }
  }

  bool await_ready() const noexcept {
    return this->done();
  }

  void await_suspend(std::experimental::coroutine_handle<> awaiting_coro) {
    this->awaiting_coros_insert_it = this->awaiting_coros.emplace_after(
        this->awaiting_coros_insert_it, awaiting_coro);
  }

  ResultT& await_resume() {
    return this->result();
  }

  const ResultT& result() const {
    auto* value_ptr = std::get_if<ResultT>(&this->value);
    if (value_ptr != nullptr) {
      return *value_ptr;
    }
    std::rethrow_exception(std::get<std::exception_ptr>(this->value));
  }

  ResultT& result() {
    auto* value_ptr = std::get_if<ResultT>(&this->value);
    if (value_ptr != nullptr) {
      return *value_ptr;
    }
    std::rethrow_exception(std::get<std::exception_ptr>(this->value));
  }

  std::exception_ptr exception() const {
    auto* exc_ptr_ptr = std::get_if<std::exception_ptr>(this->value);
    if (exc_ptr_ptr != nullptr) {
      return *exc_ptr_ptr;
    }
    return nullptr;
  }

  bool done() const noexcept {
    return !std::holds_alternative<std::monostate>(this->value);
  }
  bool has_result() const noexcept {
    return std::holds_alternative<ResultT>(this->value);
  }
  bool has_exception() const noexcept {
    return std::holds_alternative<std::exception_ptr>(this->value);
  }

  void set_result(const ResultT& v) {
    if (this->done()) {
      throw std::logic_error("set_result called on done Future");
    }
    this->value = v;
    this->resume_awaiters();
  }

  void set_result(ResultT&& v) {
    if (this->done()) {
      throw std::logic_error("set_result called on done Future");
    }
    this->value = std::move(v);
    this->resume_awaiters();
  }

  template <typename ExceptionT>
  void set_exception(ExceptionT&& exc) {
    this->set_exception(std::make_exception_ptr(exc));
  }

  void set_exception(std::exception_ptr exc) {
    if (this->done()) {
      throw std::logic_error("set_exception called on done Future");
    }
    this->value = exc;
    this->resume_awaiters();
  }

  class canceled_error : public std::runtime_error {
  public:
    canceled_error() : runtime_error("Future canceled") { }
    ~canceled_error() = default;
  };

  void cancel() {
    if (this->done()) {
      throw std::logic_error("set_exception called on done Future");
    }
    this->value = make_exception_ptr(canceled_error());
    this->resume_awaiters();
  }

  void resume_awaiters() {
    while (!this->awaiting_coros.empty()) {
      auto coro = this->awaiting_coros.front();
      this->awaiting_coros.pop_front();
      coro.resume();
    }
  }

protected:
  std::variant<std::monostate, ResultT, std::exception_ptr> value;
  std::forward_list<std::experimental::coroutine_handle<>> awaiting_coros;
  std::forward_list<std::experimental::coroutine_handle<>>::iterator awaiting_coros_insert_it;
};

template <typename ResultT>
class Future : public FutureBase<ResultT> {
public:
  using FutureBase<ResultT>::FutureBase;
};

// void can't be stored in a std::variant, so we store a const void* instead and
// hide it behind the abstraction.
template <>
class Future<void> : public FutureBase<const void*> {
public:
  using FutureBase<const void*>::FutureBase;

  void await_resume() {
    this->result();
  }

  void result() const {
    auto* value_ptr = std::get_if<const void*>(&this->value);
    if (value_ptr == nullptr) {
      std::rethrow_exception(std::get<std::exception_ptr>(this->value));
    }
  }

  void set_result() {
    if (this->done()) {
      throw std::logic_error("set_result called on done Future");
    }
    this->value = nullptr;
    this->resume_awaiters();
  }
};

} // namespace EventAsync
