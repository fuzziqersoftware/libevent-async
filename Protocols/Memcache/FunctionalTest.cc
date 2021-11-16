#include <phosg/Strings.hh>
#include <phosg/UnitTest.hh>

#include "Client.hh"

using namespace std;



EventAsync::DetachedTask main_async(EventAsync::Base& base, uint16_t port) {
  EventAsync::Memcache::Client client(base, "localhost", port);

  fprintf(stderr, "-- connect\n");
  co_await client.connect();

  fprintf(stderr, "-- noop (smoke test)\n");
  {
    uint64_t ping_usecs = co_await client.noop();
    expect_lt(ping_usecs, 1000000);
  }

  fprintf(stderr, "-- version (smoke test)\n");
  {
    string server_version = co_await client.version();
    expect(!server_version.empty());
  }

  fprintf(stderr, "-- first flush\n");
  co_await client.flush();

  fprintf(stderr, "-- set/add/replace\n");
  {
    expect_eq(true, co_await client.set("key1", 4, "value0", 6, 0));
    expect_eq(true, co_await client.set("key1", 4, "value1", 6, 1));
    expect_eq(false, co_await client.add("key1", 4, "value2", 6, 2));
    expect_eq(true, co_await client.add("key2", 4, "value2", 6, 2));
    expect_eq(false, co_await client.replace("key3", 4, "value3", 6, 3));
    expect_eq(true, co_await client.add("key3", 4, "value3", 6, 3));
  }

  fprintf(stderr, "-- get/cas\n");
  {
    auto res = co_await client.get("missing-key", 11);
    expect_eq(false, res.key_found);

    res = co_await client.get("key2", 4);
    expect_eq(true, res.key_found);
    expect_eq("value2", res.value);
    expect_ne(0, res.cas);
    expect_eq(2, res.flags);

    expect_eq(false, co_await client.set("key2", 4, "value0", 6, 0, 0, res.cas + 1));

    res = co_await client.get("key2", 4);
    expect_eq(true, res.key_found);
    expect_eq("value2", res.value);
    expect_ne(0, res.cas);
    expect_eq(2, res.flags);

    expect_eq(true, co_await client.set("key2", 4, "value0", 6, 0, 0, res.cas));

    res = co_await client.get("key2", 4);
    expect_eq(true, res.key_found);
    expect_eq("value0", res.value);
    expect_ne(0, res.cas);
    expect_eq(0, res.flags);
  }

  fprintf(stderr, "-- cas replace\n");
  {
    auto res = co_await client.get("key2", 4);
    expect_eq(true, res.key_found);
    expect_eq("value0", res.value);
    expect_ne(0, res.cas);
    expect_eq(0, res.flags);

    expect_eq(false, co_await client.replace("key2", 4, "value2", 6, 0, 0, res.cas + 1));

    res = co_await client.get("key2", 4);
    expect_eq(true, res.key_found);
    expect_eq("value0", res.value);
    expect_ne(0, res.cas);
    expect_eq(0, res.flags);

    expect_eq(true, co_await client.replace("key2", 4, "value2", 6, 2, 0, res.cas));

    res = co_await client.get("key2", 4);
    expect_eq(true, res.key_found);
    expect_eq("value2", res.value);
    expect_ne(0, res.cas);
    expect_eq(2, res.flags);
  }

  fprintf(stderr, "-- delete_key (no cas)\n");
  {
    auto res = co_await client.get("key3", 4);
    expect_eq(true, res.key_found);
    expect_eq("value3", res.value);
    expect_ne(0, res.cas);
    expect_eq(3, res.flags);

    expect_eq(true, co_await client.delete_key("key3", 4));
    res = co_await client.get("key3", 4);
    expect_eq(false, res.key_found);
  }

  fprintf(stderr, "-- delete_key (cas)\n");
  {
    auto res = co_await client.get("key2", 4);
    expect_eq(true, res.key_found);
    expect_eq("value2", res.value);
    expect_ne(0, res.cas);
    expect_eq(2, res.flags);

    expect_eq(false, co_await client.delete_key("key2", 4, res.cas + 1));
    res = co_await client.get("key2", 4);
    expect_eq(true, res.key_found);
    expect_eq("value2", res.value);
    expect_ne(0, res.cas);
    expect_eq(2, res.flags);

    expect_eq(true, co_await client.delete_key("key2", 4, res.cas));
    res = co_await client.get("key2", 4);
    expect_eq(false, res.key_found);
  }

  fprintf(stderr, "-- expiration: set/wait\n");
  {
    expect_eq(true, co_await client.set("ek1", 3, "value1", 6, 1, 1));

    auto res = co_await client.get("ek1", 3);
    expect_eq(true, res.key_found);
    expect_eq("value1", res.value);
    expect_ne(0, res.cas);
    expect_eq(1, res.flags);

    co_await base.sleep(1000000);
    res = co_await client.get("ek1", 3);
    expect_eq(false, res.key_found);
  }

  fprintf(stderr, "-- expiration: set/touch/wait\n");
  {
    expect_eq(true, co_await client.set("ek1", 3, "value1", 6, 1, 2));

    auto res = co_await client.get("ek1", 3);
    expect_eq(true, res.key_found);
    expect_eq("value1", res.value);
    expect_ne(0, res.cas);
    expect_eq(1, res.flags);

    co_await base.sleep(1000000);
    co_await client.touch("ek1", 3, 2);

    co_await base.sleep(1000000);
    res = co_await client.get("ek1", 3);
    expect_eq(true, res.key_found);
    expect_eq("value1", res.value);
    expect_ne(0, res.cas);
    expect_eq(1, res.flags);

    co_await base.sleep(1000000);
    res = co_await client.get("ek1", 3);
    expect_eq(false, res.key_found);
  }

  fprintf(stderr, "-- expiration: set/get+touch/wait\n");
  {
    expect_eq(true, co_await client.set("ek1", 3, "value1", 6, 1, 2));

    auto res = co_await client.get("ek1", 3);
    expect_eq(true, res.key_found);
    expect_eq("value1", res.value);
    expect_ne(0, res.cas);
    expect_eq(1, res.flags);

    co_await base.sleep(1000000);
    res = co_await client.get("ek1", 3, 2);
    expect_eq(true, res.key_found);
    expect_eq("value1", res.value);
    expect_ne(0, res.cas);
    expect_eq(1, res.flags);

    co_await base.sleep(1000000);
    res = co_await client.get("ek1", 3);
    expect_eq(true, res.key_found);
    expect_eq("value1", res.value);
    expect_ne(0, res.cas);
    expect_eq(1, res.flags);

    co_await base.sleep(1000000);
    res = co_await client.get("ek1", 3);
    expect_eq(false, res.key_found);
  }

  fprintf(stderr, "-- expiration: set/add\n");
  {
    expect_eq(true, co_await client.set("ek1", 3, "value1", 6, 1));

    // A failed add should not set/reset the expiration
    expect_eq(false, co_await client.add("ek1", 3, "value2", 6, 2, 1));

    co_await base.sleep(1000000);
    auto res = co_await client.get("ek1", 3);
    expect_eq(true, res.key_found);
    expect_eq("value1", res.value);
    expect_ne(0, res.cas);
    expect_eq(1, res.flags);
  }

  fprintf(stderr, "-- increment\n");
  {
    expect_eq(true, co_await client.set("ik1", 3, "123", 3));
    expect_eq(125, co_await client.increment("ik1", 3, 2));
    expect_eq(10, co_await client.increment("ik2", 3, 2, 10));
    expect_eq(2, co_await client.increment("ik3", 3, 2));

    expect_eq(100, co_await client.increment("ik1", 3, 25, 0, 0, true));
    expect_eq(0, co_await client.increment("ik1", 3, 200, 0, 0, true));
  }

  fprintf(stderr, "-- append/prepend\n");
  {
    expect_eq(true, co_await client.set("ak1", 3, "456", 3));
    co_await client.append("ak1", 3, "789", 3);
    expect_eq("456789", (co_await client.get("ak1", 3)).value);
    co_await client.append("ak1", 3, "123", 3, true);
    expect_eq("123456789", (co_await client.get("ak1", 3)).value);
  }

  fprintf(stderr, "-- stats (smoke test)\n");
  co_await client.stats();

  fprintf(stderr, "-- final flush\n");
  {
    expect_eq(true, co_await client.set("f1", 2, "456", 3));
    co_await client.flush(2);
    expect_eq("456", (co_await client.get("f1", 2)).value);
    co_await base.sleep(2000000);
    expect_eq(false, (co_await client.get("f1", 2)).key_found);
  }
}

int main(int argc, char** argv) {
  if (argc < 1 || argc > 2) {
    throw invalid_argument("Usage: FunctionalTest [port]");
  }

  uint16_t port = (argc == 2) ? atoi(argv[1]) : 11211;

  EventAsync::Base base;
  main_async(base, port);
  base.run();
  return 0;
}
