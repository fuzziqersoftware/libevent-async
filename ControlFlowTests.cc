#include <unordered_set>
#include <experimental/coroutine>
#include <phosg/UnitTest.hh>
#include <phosg/Time.hh>

#include "Task.hh"
#include "Base.hh"

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

  return 0;
}
