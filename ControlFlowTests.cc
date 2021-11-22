#include <inttypes.h>

#include <unordered_set>
#include <experimental/coroutine>
#include <phosg/UnitTest.hh>
#include <phosg/Network.hh>
#include <phosg/Time.hh>

#include "Task.hh"
#include "Base.hh"
#include "Buffer.hh"

using namespace std;
using namespace EventAsync;



struct Timer {
  uint64_t start;
  uint64_t low;
  uint64_t high;

  Timer(uint64_t low = 0, uint64_t high = 0)
    : start(now()), low(low), high(high) { }
  ~Timer() noexcept(false) {
    uint64_t duration = now() - this->start;
    fprintf(stderr, "---- duration: %" PRIu64 " usecs\n", duration);
    if (this->low) {
      expect_ge(duration, this->low);
    }
    if (this->high) {
      expect_le(duration, this->high);
    }
  }
};



Task<size_t> test_returns_fn1() {
  co_return 5;
}

Task<size_t> test_returns_fn2() {
  size_t ret = co_await test_returns_fn1();
  co_return ret + 4;
}

DetachedTask test_returns(Base& base) {
  size_t v = co_await test_returns_fn2();
  expect_eq(v, 9);
}



Task<void> test_exceptions_fn1() {
  throw std::runtime_error("exc");
  co_return;
}

Task<void> test_exceptions_fn2() {
  co_await test_returns_fn1();
  co_return;
}

DetachedTask test_exceptions(Base& base) {
  try {
    co_await test_returns_fn2();
  } catch (const runtime_error& e) {
    expect(strcmp(e.what(), "exc"));
  }
}



DetachedTask test_timeouts(Base& base) {
  uint64_t start = now();
  co_await base.sleep(1000000);
  expect_ge(now() - start, 1000000);
}



Task<void> sleep_task(Base& base, uint64_t usecs) {
  co_await base.sleep(usecs);
}

DetachedTask test_all_sleep(Base& base) {
  vector<Task<void>> tasks;
  tasks.emplace_back(sleep_task(base, 1000000));
  tasks.emplace_back(sleep_task(base, 2000000));
  tasks.emplace_back(sleep_task(base, 3000000));

  {
    Timer t(3000000, 4000000);
    co_await all(tasks.begin(), tasks.end());
  }
  for (const auto& task : tasks) {
    expect(task.done());
  }

  // None of these should throw
  co_await tasks[0];
  co_await tasks[1];
  co_await tasks[2];
}



Task<void> test_all_sleep_exception_task(Base& base, uint64_t usecs) {
  co_await base.sleep(usecs);
  if (usecs == 1000000) {
    throw runtime_error("oops");
  }
}

DetachedTask test_all_sleep_exception(Base& base) {
  vector<Task<void>> tasks;
  tasks.emplace_back(test_all_sleep_exception_task(base, 1000000));
  tasks.emplace_back(test_all_sleep_exception_task(base, 2000000));
  tasks.emplace_back(test_all_sleep_exception_task(base, 3000000));

  {
    Timer t(3000000, 4000000);
    co_await all(tasks.begin(), tasks.end());
  }
  for (const auto& task : tasks) {
    expect(task.done());
  }

  // We should still be able to get the exception even after all() returns
  try {
    co_await tasks[0];
    expect(false);
  } catch (const runtime_error&) { }
  co_await tasks[1];
  co_await tasks[2];
}



Task<void> test_all_network_fn(
    Base& base,
    int fd,
    size_t num_iterations,
    bool read_first) {

  static const string chunk_data(1024, '\0');
  Buffer buf(base);
  bool should_read = read_first;
  for (size_t z = 0; z < num_iterations; z++) {
    if (should_read) {
      co_await buf.read(fd, chunk_data.size());
      expect_eq(chunk_data, buf.remove(chunk_data.size()));
      buf.drain_all();
    } else {
      buf.add_reference(chunk_data.data(), chunk_data.size());
      co_await buf.write(fd);
      buf.drain_all();
    }
  }
}

DetachedTask test_all_network(Base& base) {
  auto fds = socketpair();

  vector<Task<void>> tasks;
  tasks.emplace_back(test_all_network_fn(base, fds.first, 5, true));
  tasks.emplace_back(test_all_network_fn(base, fds.second, 5, false));

  {
    Timer t;
    co_await all(tasks.begin(), tasks.end());
  }
  for (const auto& task : tasks) {
    expect(task.done());
  }
  co_await tasks[0];
  co_await tasks[1];
}



DetachedTask test_any_sleep(Base& base) {
  vector<Task<void>> tasks;
  tasks.emplace_back(sleep_task(base, 1000000));
  tasks.emplace_back(sleep_task(base, 2000000));
  tasks.emplace_back(sleep_task(base, 3000000));

  Task<void>* completed_task;
  {
    Timer t(1000000, 2000000);
    completed_task = co_await any(tasks.begin(), tasks.end());
  }
  expect_eq(completed_task, &tasks[0]);
  expect(completed_task->done());
  expect(tasks[0].done());
  expect(!tasks[1].done());
  expect(!tasks[2].done());

  // This should return almost immediately
  {
    Timer t(0, 1000000);
    completed_task = co_await any(tasks.begin(), tasks.end());
  }
  expect_eq(completed_task, &tasks[0]);
  expect(completed_task->done());
  expect(tasks[0].done());
  expect(!tasks[1].done());
  expect(!tasks[2].done());

  {
    // duration could be slightly less than 1 second since we did some extra
    // work while the other tasks were still "running"
    Timer t(0, 2000000);
    completed_task = co_await any(tasks.begin() + 1, tasks.end());
  }
  expect_eq(completed_task, &tasks[1]);
  expect(completed_task->done());
  expect(tasks[0].done());
  expect(tasks[1].done());
  expect(!tasks[2].done());

  {
    // duration could be slightly less than 1 second since we did some extra
    // work while the other tasks were still "running"
    Timer t(0, 2000000);
    completed_task = co_await any(tasks.begin() + 2, tasks.end());
  }
  expect_eq(completed_task, &tasks[2]);
  expect(completed_task->done());
  expect(tasks[0].done());
  expect(tasks[1].done());
  expect(tasks[2].done());

  // None of these should throw
  co_await tasks[0];
  co_await tasks[1];
  co_await tasks[2];
}



DetachedTask test_all_limit_sleep(Base& base) {
  vector<Task<void>> tasks;
  tasks.emplace_back(sleep_task(base, 1000000));
  tasks.emplace_back(sleep_task(base, 1000000));
  tasks.emplace_back(sleep_task(base, 1000000));
  tasks.emplace_back(sleep_task(base, 1000000));
  tasks.emplace_back(sleep_task(base, 1000000));

  {
    Timer t(3000000, 4000000);
    co_await all_limit(tasks.begin(), tasks.end(), 2);
  }
  for (const auto& task : tasks) {
    expect(task.done());
  }
  co_await tasks[0];
  co_await tasks[1];
  co_await tasks[2];
  co_await tasks[3];
  co_await tasks[4];
}



Task<void> test_future_void_set(Base& base, Future<void>& f) {
  co_await base.sleep(1000000);
  f.set_result();
}

Task<void> test_future_void_await(Future<void>& f) {
  co_await f;
}

DetachedTask test_future_void(Base& base) {
  {
    fprintf(stderr, "---- no awaiters\n");
    Future<void> f;
    {
      Timer t(1000000, 2000000);
      co_await test_future_void_set(base, f);
    }
    expect(f.has_result());
  }

  {
    fprintf(stderr, "---- one awaiter\n");
    Future<void> f;
    vector<Task<void>> tasks;
    tasks.emplace_back(test_future_void_set(base, f));
    tasks.emplace_back(test_future_void_await(f));
    {
      Timer t(1000000, 2000000);
      co_await all(tasks.begin(), tasks.end());
    }
    expect(f.has_result());
  }

  {
    fprintf(stderr, "---- multiple awaiters\n");
    Future<void> f;
    vector<Task<void>> tasks;
    tasks.emplace_back(test_future_void_set(base, f));
    tasks.emplace_back(test_future_void_await(f));
    tasks.emplace_back(test_future_void_await(f));
    tasks.emplace_back(test_future_void_await(f));
    {
      Timer t(1000000, 2000000);
      co_await all(tasks.begin(), tasks.end());
    }
    expect(f.has_result());
  }
}



Task<void> test_future_void_set_exc(Base& base, Future<void>& f) {
  co_await base.sleep(1000000);
  f.set_exception(make_exception_ptr(out_of_range("nope")));
}

Task<void> test_future_void_await_exc(Future<void>& f) {
  try {
    co_await f;
    throw logic_error("co_await f should have thrown but it did not");
  } catch (const out_of_range&) { }
}

DetachedTask test_future_void_exc(Base& base) {
  {
    fprintf(stderr, "---- no awaiters\n");
    Future<void> f;
    {
      Timer t(1000000, 2000000);
      co_await test_future_void_set_exc(base, f);
    }
    expect(f.has_exception());
    // awaiting the Future after it has an exception should throw immediately
    co_await test_future_void_await_exc(f);
  }

  {
    fprintf(stderr, "---- one awaiter\n");
    Future<void> f;
    vector<Task<void>> tasks;
    tasks.emplace_back(test_future_void_set_exc(base, f));
    tasks.emplace_back(test_future_void_await_exc(f));
    {
      Timer t(1000000, 2000000);
      co_await all(tasks.begin(), tasks.end());
    }
    expect(f.has_exception());
  }

  {
    fprintf(stderr, "---- multiple awaiters\n");
    Future<void> f;
    vector<Task<void>> tasks;
    tasks.emplace_back(test_future_void_set_exc(base, f));
    tasks.emplace_back(test_future_void_await_exc(f));
    tasks.emplace_back(test_future_void_await_exc(f));
    tasks.emplace_back(test_future_void_await_exc(f));
    {
      Timer t(1000000, 2000000);
      co_await all(tasks.begin(), tasks.end());
    }
    expect(f.has_exception());
  }
}



template <typename FutureT>
Task<int64_t> test_future_set_value(Base& base, FutureT& f, int64_t value) {
  co_await base.sleep(1000000);
  f.set_result(value);
  co_return move(value);
}

template <typename FutureT>
Task<int64_t> test_future_await(Base& base, FutureT& f) {
  co_return move(co_await f);
}

DetachedTask test_future_value(Base& base) {
  {
    fprintf(stderr, "---- no awaiters\n");
    Future<int64_t> f;
    {
      Timer t(1000000, 2000000);
      expect_eq(co_await test_future_set_value(base, f, 5), 5);
    }
    expect_eq(f.result(), 5);
    expect_eq(co_await f, 5);
  }

  {
    fprintf(stderr, "---- one awaiter\n");
    Future<int64_t> f;
    vector<Task<int64_t>> tasks;
    tasks.emplace_back(test_future_set_value(base, f, 6));
    tasks.emplace_back(test_future_await(base, f));
    {
      Timer t(1000000, 2000000);
      co_await all(tasks.begin(), tasks.end());
    }
    expect_eq(f.result(), 6);
    expect_eq(tasks[0].result(), 6);
    expect_eq(tasks[1].result(), 6);
    expect_eq(co_await f, 6);
  }

  {
    fprintf(stderr, "---- multiple awaiters\n");
    Future<int64_t> f;
    vector<Task<int64_t>> tasks;
    tasks.emplace_back(test_future_set_value(base, f, 7));
    tasks.emplace_back(test_future_await(base, f));
    tasks.emplace_back(test_future_await(base, f));
    tasks.emplace_back(test_future_await(base, f));
    {
      Timer t(1000000, 2000000);
      co_await all(tasks.begin(), tasks.end());
    }
    expect_eq(f.result(), 7);
    expect_eq(tasks[0].result(), 7);
    expect_eq(tasks[1].result(), 7);
    expect_eq(tasks[2].result(), 7);
    expect_eq(tasks[3].result(), 7);
    expect_eq(co_await f, 7);
  }
}

DetachedTask test_deferred_future_value(Base& base) {
  {
    fprintf(stderr, "---- no awaiters\n");
    DeferredFuture<int64_t> f(base);
    {
      Timer t(1000000, 2000000);
      expect_eq(co_await test_future_set_value(base, f, 5), 5);
    }
    expect_eq(f.result(), 5);
    expect_eq(co_await f, 5);
  }

  {
    fprintf(stderr, "---- one awaiter\n");
    DeferredFuture<int64_t> f(base);
    vector<Task<int64_t>> tasks;
    tasks.emplace_back(test_future_set_value(base, f, 6));
    tasks.emplace_back(test_future_await(base, f));
    {
      Timer t(1000000, 2000000);
      co_await all(tasks.begin(), tasks.end());
    }
    expect_eq(f.result(), 6);
    expect_eq(tasks[0].result(), 6);
    expect_eq(tasks[1].result(), 6);
    expect_eq(co_await f, 6);
  }

  {
    fprintf(stderr, "---- multiple awaiters\n");
    DeferredFuture<int64_t> f(base);
    vector<Task<int64_t>> tasks;
    tasks.emplace_back(test_future_set_value(base, f, 7));
    tasks.emplace_back(test_future_await(base, f));
    tasks.emplace_back(test_future_await(base, f));
    tasks.emplace_back(test_future_await(base, f));
    {
      Timer t(1000000, 2000000);
      co_await all(tasks.begin(), tasks.end());
    }
    expect_eq(f.result(), 7);
    expect_eq(tasks[0].result(), 7);
    expect_eq(tasks[1].result(), 7);
    expect_eq(tasks[2].result(), 7);
    expect_eq(tasks[3].result(), 7);
    expect_eq(co_await f, 7);
  }
}



int main(int argc, char** argv) {
  Base base;

  fprintf(stderr, "-- test_returns\n");
  test_returns(base);
  base.run();

  fprintf(stderr, "-- test_exceptions\n");
  test_exceptions(base);
  base.run();

  fprintf(stderr, "-- test_timeouts\n");
  test_timeouts(base);
  base.run();

  fprintf(stderr, "-- test_all_sleep\n");
  test_all_sleep(base);
  base.run();

  fprintf(stderr, "-- test_all_sleep_exception\n");
  test_all_sleep_exception(base);
  base.run();

  fprintf(stderr, "-- test_all_network\n");
  test_all_network(base);
  base.run();

  fprintf(stderr, "-- test_any_sleep\n");
  test_any_sleep(base);
  base.run();

  fprintf(stderr, "-- test_all_limit_sleep\n");
  test_all_limit_sleep(base);
  base.run();

  fprintf(stderr, "-- test_future_void\n");
  test_future_void(base);
  base.run();

  fprintf(stderr, "-- test_future_void_exc\n");
  test_future_void_exc(base);
  base.run();

  fprintf(stderr, "-- test_future_value\n");
  test_future_value(base);
  base.run();

  fprintf(stderr, "-- test_deferred_future_value\n");
  test_deferred_future_value(base);
  base.run();

  fprintf(stderr, "-- all tests passed\n");

  return 0;
}
