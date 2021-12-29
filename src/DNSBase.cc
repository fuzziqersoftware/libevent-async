#include "DNSBase.hh"

using namespace std;
using namespace std::experimental;



namespace EventAsync {

DNSBase::DNSBase(Base& base)
  : dns_base(evdns_base_new(base.base, true)) { }

DNSBase::DNSBase(DNSBase&& other) : dns_base(other.dns_base) {
  other.dns_base = nullptr;
}

DNSBase& DNSBase::operator=(DNSBase&& other) {
  this->dns_base = other.dns_base;
  other.dns_base = nullptr;
  return *this;
}

DNSBase::~DNSBase() {
  if (this->dns_base) {
    evdns_base_free(this->dns_base, true);
  }
}

void DNSBase::resolv_conf_parse(int flags, const char* filename) {
  if (evdns_base_resolv_conf_parse(this->dns_base, flags, filename)) {
    throw runtime_error("evdns_base_resolv_conf_parse");
  }
}

#ifdef WIN32
void DNSBase::config_windows_nameservers() {
  if (evdns_base_config_windows_nameservers(this->dns_base)) {
    throw runtime_error("evdns_base_config_windows_nameservers");
  }
}
#endif

void DNSBase::nameserver_sockaddr_add(const struct sockaddr* sa, socklen_t len,
    unsigned flags) {
  if (evdns_base_nameserver_sockaddr_add(this->dns_base, sa, len, flags)) {
    throw runtime_error("evdns_base_nameserver_sockaddr_add");
  }
}

void DNSBase::nameserver_ip_add(const char* ip_as_string) {
  if (evdns_base_nameserver_ip_add(this->dns_base, ip_as_string)) {
    throw runtime_error("evdns_base_nameserver_ip_add");
  }
}

void DNSBase::load_hosts(const char* hosts_fname) {
  if (evdns_base_load_hosts(this->dns_base, hosts_fname)) {
    throw runtime_error("evdns_base_load_hosts");
  }
}

void DNSBase::clear_nameservers_and_suspend() {
  if (evdns_base_clear_nameservers_and_suspend(this->dns_base)) {
    throw runtime_error("evdns_base_clear_nameservers_and_suspend");
  }
}

void DNSBase::resume() {
  if (evdns_base_resume(this->dns_base)) {
    throw runtime_error("evdns_base_resume");
  }
}

void DNSBase::search_clear() {
  return evdns_base_search_clear(this->dns_base);
}

void DNSBase::search_add(const char *domain) {
  return evdns_base_search_add(this->dns_base, domain);
}

void DNSBase::search_ndots_set(int ndots) {
  return evdns_base_search_ndots_set(this->dns_base, ndots);
}

void DNSBase::set_option(const char* option, const char* val) {
  if (evdns_base_set_option(this->dns_base, option, val)) {
    throw runtime_error("evdns_base_set_option");
  }
}

int DNSBase::count_nameservers() {
  return evdns_base_count_nameservers(this->dns_base);
}

struct evdns_getaddrinfo_request* DNSBase::getaddrinfo(
    const char* nodename,
    const char* servname,
    const struct evutil_addrinfo* hints_in,
    void (*cb)(int result, struct evutil_addrinfo* res, void* arg),
    void* cbarg) {
  return evdns_getaddrinfo(this->dns_base, nodename, servname, hints_in, cb, cbarg);
}

void DNSBase::getaddrinfo_cancel(struct evdns_getaddrinfo_request* req) {
  return evdns_getaddrinfo_cancel(req);
}



DNSBase::LookupAwaiterBase::LookupAwaiterBase(
    DNSBase& dns_base, const void* target, int flags)
  : dns_base(dns_base),
    complete(false),
    target(target),
    flags(flags),
    coro(nullptr) { }

bool DNSBase::LookupAwaiterBase::await_ready() const noexcept {
  return this->complete;
}

void DNSBase::LookupAwaiterBase::await_suspend(coroutine_handle<> coro) {
  this->coro = coro;
  this->start_request();
}

void DNSBase::LookupAwaiterBase::dispatch_on_request_complete(
    int result, char type, int count, int ttl, void* addresses, void* arg) {
  auto* aw = reinterpret_cast<LookupAwaiterBase*>(arg);
  aw->on_request_complete(result, type, count, ttl, addresses);
  aw->complete = true;
  aw->coro.resume();
}

DNSBase::LookupResult<in_addr>&& DNSBase::LookupIPv4Awaiter::await_resume() {
  return move(this->result);
}

void DNSBase::LookupIPv4Awaiter::start_request() {
  if (evdns_base_resolve_ipv4(
      this->dns_base.dns_base,
      reinterpret_cast<const char*>(this->target),
      this->flags,
      &LookupAwaiterBase::dispatch_on_request_complete,
      this) == nullptr) {
    throw runtime_error("evdns_base_resolve_ipv4 failed");
  }
}

void DNSBase::LookupIPv4Awaiter::on_request_complete(
    int result, char type, int count, int ttl, const void* addresses) {
  this->result.result_code = result;
  if (this->result.result_code == DNS_ERR_NONE) {
    if (type != DNS_IPv4_A) {
      throw logic_error("IPv4 lookup did not result in A record");
    }
    this->result.ttl = ttl;
    const in_addr* addrs = reinterpret_cast<const in_addr*>(addresses);
    this->result.results.reserve(count);
    for (int x = 0; x < count; x++) {
      this->result.results.emplace_back(addrs[x]);
    }
  }
}

DNSBase::LookupResult<in6_addr>&& DNSBase::LookupIPv6Awaiter::await_resume() {
  return move(this->result);
}

void DNSBase::LookupIPv6Awaiter::start_request() {
  if (evdns_base_resolve_ipv6(
      this->dns_base.dns_base,
      reinterpret_cast<const char*>(this->target),
      this->flags,
      &LookupAwaiterBase::dispatch_on_request_complete,
      this) == nullptr) {
    throw runtime_error("evdns_base_resolve_ipv6 failed");
  }
}

void DNSBase::LookupIPv6Awaiter::on_request_complete(
    int result, char type, int count, int ttl, const void* addresses) {
  this->result.result_code = result;
  if (this->result.result_code == DNS_ERR_NONE) {
    if (type != DNS_IPv6_AAAA) {
      throw logic_error("IPv6 lookup did not result in AAAA record");
    }
    this->result.ttl = ttl;
    const in6_addr* addrs = reinterpret_cast<const in6_addr*>(addresses);
    this->result.results.reserve(count);
    for (int x = 0; x < count; x++) {
      this->result.results.emplace_back(addrs[x]);
    }
  }
}

DNSBase::LookupResult<string>&& DNSBase::LookupReverseAwaiterBase::await_resume() {
  return move(this->result);
}

void DNSBase::LookupReverseAwaiterBase::on_request_complete(
    int result, char type, int count, int ttl, const void* addresses) {
  this->result.result_code = result;
  if (this->result.result_code == DNS_ERR_NONE) {
    if (type != DNS_PTR) {
      throw logic_error("reverse lookup did not result in PTR record");
    }
    this->result.ttl = ttl;
    const char* const * addrs = reinterpret_cast<const char* const *>(addresses);
    this->result.results.reserve(count);
    for (int x = 0; x < count; x++) {
      this->result.results.emplace_back(addrs[x]);
    }
  }
}

void DNSBase::LookupReverseIPv4Awaiter::start_request() {
  if (evdns_base_resolve_reverse(
      this->dns_base.dns_base,
      reinterpret_cast<const in_addr*>(this->target),
      this->flags,
      &LookupAwaiterBase::dispatch_on_request_complete,
      this) == nullptr) {
    throw runtime_error("evdns_base_resolve_reverse failed");
  }
}

void DNSBase::LookupReverseIPv6Awaiter::start_request() {
  if (evdns_base_resolve_reverse_ipv6(
      this->dns_base.dns_base,
      reinterpret_cast<const in6_addr*>(this->target),
      this->flags,
      &LookupAwaiterBase::dispatch_on_request_complete,
      this) == nullptr) {
    throw runtime_error("evdns_base_resolve_reverse_ipv6 failed");
  }
}

DNSBase::LookupIPv4Awaiter DNSBase::resolve_ipv4(const char* name, int flags) {
  return LookupIPv4Awaiter(*this, name, flags);
}

DNSBase::LookupIPv6Awaiter DNSBase::resolve_ipv6(const char* name, int flags) {
  return LookupIPv6Awaiter(*this, name, flags);
}

DNSBase::LookupReverseIPv4Awaiter DNSBase::resolve_reverse_ipv4(
    const struct in_addr* addr, int flags) {
  return LookupReverseIPv4Awaiter(*this, addr, flags);
}

DNSBase::LookupReverseIPv6Awaiter DNSBase::resolve_reverse_ipv6(
    const struct in6_addr* addr, int flags) {
  return LookupReverseIPv6Awaiter(*this, addr, flags);
}

const char* err_to_string(int err) {
  return evdns_err_to_string(err);
}

} // namespace EventAsync
