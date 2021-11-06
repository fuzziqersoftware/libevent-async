#pragma once

#include <event2/event.h>
#include <event2/dns.h>

#include <functional>
#include <memory>

#include "EventBase.hh"



// TODO: support evdns_server functions

class EvDNSBase {
public:
  EvDNSBase(EventBase& base);
  EvDNSBase(const EvDNSBase& base) = delete;
  EvDNSBase(EvDNSBase&& base);
  EvDNSBase& operator=(const EvDNSBase& base) = delete;
  EvDNSBase& operator=(EvDNSBase&& base);
  ~EvDNSBase();

  void resolv_conf_parse(int flags, const char* filename);
#ifdef WIN32
  void config_windows_nameservers();
#endif

  void nameserver_sockaddr_add(const struct sockaddr* sa, socklen_t len,
      unsigned flags);
  void nameserver_ip_add(const char* ip_as_string);
  void load_hosts(const char* hosts_fname);

  void clear_nameservers_and_suspend();
  void resume();

  void search_clear();
  void search_add(const char *domain);
  void search_ndots_set(int ndots);

  void set_option(const char* option, const char* val);
  int count_nameservers();

// TODO: replace all of these callback functions with awaiters

  struct evdns_getaddrinfo_request* getaddrinfo(
      const char* nodename, const char* servname,
      const struct evutil_addrinfo* hints_in,
      void (*cb)(int result, struct evutil_addrinfo* res, void* arg),
      void* cbarg);
  void getaddrinfo_cancel(struct evdns_getaddrinfo_request*);

  struct evdns_request* resolve_ipv4(const char* name, int flags,
      evdns_callback_type callback, void* ctx);
  struct evdns_request* resolve_ipv6(const char* name, int flags,
      evdns_callback_type callback, void* ctx);
  struct evdns_request* resolve_reverse(const struct in_addr* in, int flags,
      evdns_callback_type callback, void* ctx);
  struct evdns_request* resolve_reverse_ipv6(const struct in6_addr* in,
      int flags, evdns_callback_type callback, void* ctx);

  void cancel_request(struct evdns_request* req);

  static const char* err_to_string(int err);

  struct evdns_base* dns_base;
};
