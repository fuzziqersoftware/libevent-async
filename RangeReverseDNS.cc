#include <arpa/inet.h>
#include <unistd.h>

#include <experimental/coroutine>
#include <phosg/Network.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Encoding.hh>

#include "Task.hh"
#include "Base.hh"
#include "DNSBase.hh"

using namespace std;
using namespace EventAsync;



Task<void> print_reverse_lookup_result(DNSBase& dns_base, uint32_t s_addr) {
  struct in_addr addr;
  addr.s_addr = htonl(s_addr);
  auto result = co_await dns_base.resolve_reverse_ipv4(&addr);
  fprintf(stdout, "%u.%u.%u.%u => (%d)",
      (s_addr >> 24) & 0xFF,
      (s_addr >> 16) & 0xFF,
      (s_addr >> 8) & 0xFF,
      s_addr & 0xFF,
      result.result_code);
  if (result.result_code == 0) {
    fprintf(stdout, " ttl=%d [", result.ttl);
    bool is_first = true;
    for (const string& name : result.results) {
      if (!is_first) {
        fputc(',', stdout);
      }
      is_first = false;
      fwritex(stdout, name);
    }
    fputs("]\n", stdout);
  } else {
    fputc('\n', stdout);
  }
}

DetachedTask print_reverse_range_lookup_results(
    EventAsync::Base& base,
    uint32_t s_addr_base,
    uint8_t scope_bits,
    uint64_t delay_usecs) {
  s_addr_base &= ~((1 << (32 - scope_bits)) - 1);

  DNSBase dns_base(base);
  for (size_t x = 0; x < (1 << (32 - scope_bits)); x++) {
    if (x) {
      co_await base.sleep(delay_usecs);
    }
    co_await print_reverse_lookup_result(dns_base, s_addr_base | x);
  }
}

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    throw invalid_argument("Usage: RangeReverseDNS <ip-addr>/<scope-bits> [delay-usecs]\n");
  }
  string arg = argv[1];
  size_t slash_pos = arg.find('/');
  uint32_t s_addr_base = 0;
  uint8_t scope_bits = 32;
  if (slash_pos != string::npos) {
    string addr_str = arg.substr(0, slash_pos);
    s_addr_base = bswap32(inet_addr(addr_str.c_str()));
    scope_bits = atoi(&arg[slash_pos + 1]);
  } else {
    s_addr_base = bswap32(inet_addr(arg.c_str()));
  }

  uint64_t delay_usecs = (argc >= 3) ? strtoull(argv[2], nullptr, 0) : 1000000;

  Base base;
  print_reverse_range_lookup_results(base, s_addr_base, scope_bits, delay_usecs);
  base.run();
  return 0;
}
