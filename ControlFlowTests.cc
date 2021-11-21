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

  uint64_t start = now();
  co_await all(tasks.begin(), tasks.end());
  uint64_t duration = now() - start;
  fprintf(stderr, "---- duration: %" PRIu64 " usecs\n", duration);
  expect_ge(duration, 3000000);
  expect_le(duration, 4000000);
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

  uint64_t start = now();
  co_await all(tasks.begin(), tasks.end());
  uint64_t duration = now() - start;
  fprintf(stderr, "---- duration: %" PRIu64 " usecs\n", duration);
  expect_ge(duration, 3000000);
  expect_le(duration, 4000000);
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

  uint64_t start = now();
  co_await all(tasks.begin(), tasks.end());
  uint64_t duration = now() - start;
  fprintf(stderr, "---- duration: %" PRIu64 " usecs\n", duration);
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

  uint64_t start = now();
  auto* completed_task = co_await any(tasks.begin(), tasks.end());
  uint64_t duration = now() - start;
  fprintf(stderr, "---- duration: %" PRIu64 " usecs\n", duration);
  expect_ge(duration, 1000000);
  expect_lt(duration, 2000000);
  expect_eq(completed_task, &tasks[0]);
  expect(completed_task->done());
  expect(tasks[0].done());
  expect(!tasks[1].done());
  expect(!tasks[2].done());

  // This should return almost immediately
  start = now();
  completed_task = co_await any(tasks.begin(), tasks.end());
  duration = now() - start;
  fprintf(stderr, "---- duration: %" PRIu64 " usecs\n", duration);
  expect_lt(duration, 1000000);
  expect_eq(completed_task, &tasks[0]);
  expect(completed_task->done());
  expect(tasks[0].done());
  expect(!tasks[1].done());
  expect(!tasks[2].done());

  start = now();
  completed_task = co_await any(tasks.begin() + 1, tasks.end());
  duration = now() - start;
  fprintf(stderr, "---- duration: %" PRIu64 " usecs\n", duration);
  // duration could be slightly less than 1 second since we did some extra work
  // while the other tasks were still "running"
  expect_lt(duration, 2000000);
  expect_eq(completed_task, &tasks[1]);
  expect(completed_task->done());
  expect(tasks[0].done());
  expect(tasks[1].done());
  expect(!tasks[2].done());

  start = now();
  completed_task = co_await any(tasks.begin() + 2, tasks.end());
  duration = now() - start;
  fprintf(stderr, "---- duration: %" PRIu64 " usecs\n", duration);
  // duration could be slightly less than 1 second since we did some extra work
  // while the other tasks were still "running"
  expect_lt(duration, 2000000);
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

  uint64_t start = now();
  co_await all_limit(tasks.begin(), tasks.end(), 2);
  uint64_t duration = now() - start;
  fprintf(stderr, "---- duration: %" PRIu64 " usecs\n", duration);
  expect_ge(duration, 3000000);
  expect_le(duration, 4000000);
  for (const auto& task : tasks) {
    expect(task.done());
  }
  co_await tasks[0];
  co_await tasks[1];
  co_await tasks[2];
  co_await tasks[3];
  co_await tasks[4];
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

  fprintf(stderr, "-- all tests passed\n");

  return 0;
}
