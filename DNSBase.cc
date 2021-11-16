#include "DNSBase.hh"

using namespace std;



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

struct evdns_request* DNSBase::resolve_ipv4(
    const char* name, int flags, evdns_callback_type cb, void* ctx) {
  return evdns_base_resolve_ipv4(this->dns_base, name, flags, cb, ctx);
}

struct evdns_request* DNSBase::resolve_ipv6(
    const char* name, int flags, evdns_callback_type cb, void* ctx) {
  return evdns_base_resolve_ipv6(this->dns_base, name, flags, cb, ctx);
}

struct evdns_request* DNSBase::resolve_reverse(
    const struct in_addr* in, int flags, evdns_callback_type cb, void* ctx) {
  return evdns_base_resolve_reverse(this->dns_base, in, flags, cb, ctx);
}

struct evdns_request* DNSBase::resolve_reverse_ipv6(
    const struct in6_addr* in, int flags, evdns_callback_type cb, void* ctx) {
  return evdns_base_resolve_reverse_ipv6(this->dns_base, in, flags, cb, ctx);
}

void DNSBase::cancel_request(struct evdns_request* req) {
  return evdns_cancel_request(this->dns_base, req);
}

const char* err_to_string(int err) {
  return evdns_err_to_string(err);
}

} // namespace EventAsync
