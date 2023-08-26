#pragma once

#include <coroutine>
#include <deque>
#include <forward_list>



namespace EventAsync {

// TODO: Should we support Channel<void>? Currently we don't, but it could turn
// out to be useful in rare scenarios. To implement it, presumably we'd just
// replace the deque with a count of excess send() calls.

template <typename ItemT>
class Channel {
public:
  Channel() : awaiting_coros_insert_it(this->awaiting_coros.before_begin()) { }
  Channel(const Channel&) = delete;
  Channel(Channel&&) = default;
  Channel& operator=(const Channel&) = delete;
  Channel& operator=(Channel&&) = default;
  virtual ~Channel() noexcept(false) {
    // Throwing in a destructor is a Bad Idea. But it's also a Bad Idea to let
    // a Channel be destroyed while someone is waiting on it!
    if (!this->awaiting_coros.empty()) {
      throw std::logic_error("Channel destroyed with awaiters present");
    }
  }

  bool empty() const noexcept {
    return this->queue.empty();
  }
  size_t size() const noexcept {
    return this->queue.size();
  }

  class ReadAwaiter {
  public:
    ReadAwaiter(Channel& c) : c(c) { }

    bool await_ready() const noexcept {
      return !this->c.queue.empty();
    }

    void await_suspend(std::coroutine_handle<> awaiting_coro) {
      this->c.awaiting_coros_insert_it = this->c.awaiting_coros.emplace_after(
          this->c.awaiting_coros_insert_it, awaiting_coro);
    }

    ItemT await_resume() {
      ItemT ret = std::move(this->c.queue.front());
      this->c.queue.pop_front();
      return ret;
    }

  private:
    Channel& c;
  };

  ReadAwaiter read() {
    return ReadAwaiter(*this);
  }

  void write(const ItemT& v) {
    this->queue.emplace_back(v);
    this->resume_awaiter();
  }

  void write(ItemT&& v) {
    this->queue.emplace_back(std::move(v));
    this->resume_awaiter();
  }

protected:
  void resume_awaiter() {
    if (!this->awaiting_coros.empty()) {
      auto coro = this->awaiting_coros.front();
      this->awaiting_coros.pop_front();
      coro.resume();
    }
  }

  std::deque<ItemT> queue;
  std::forward_list<std::coroutine_handle<>> awaiting_coros;
  std::forward_list<std::coroutine_handle<>>::iterator awaiting_coros_insert_it;
};

} // namespace EventAsync
