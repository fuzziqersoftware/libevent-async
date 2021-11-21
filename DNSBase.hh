#pragma once

#include <event2/event.h>
#include <event2/dns.h>

#include <functional>
#include <memory>

#include "Base.hh"



namespace EventAsync {

// TODO: support evdns_server functions

class DNSBase {
public:
  DNSBase(Base& base);
  DNSBase(const DNSBase& base) = delete;
  DNSBase(DNSBase&& base);
  DNSBase& operator=(const DNSBase& base) = delete;
  DNSBase& operator=(DNSBase&& base);
  ~DNSBase();

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

  // TODO: write an awaiter for getaddrinfo

  struct evdns_getaddrinfo_request* getaddrinfo(
      const char* nodename, const char* servname,
      const struct evutil_addrinfo* hints_in,
      void (*cb)(int result, struct evutil_addrinfo* res, void* arg),
      void* cbarg);
  void getaddrinfo_cancel(struct evdns_getaddrinfo_request*);



  template <typename ResultT>
  struct LookupResult {
    int result_code;
    int ttl;
    std::vector<ResultT> results;
  };

  class LookupAwaiterBase {
  public:
    LookupAwaiterBase(DNSBase& dns_base, const void* target, int flags);
    bool await_ready() const noexcept;
    void await_suspend(std::experimental::coroutine_handle<> coro);

  protected:
    virtual void start_request() = 0;
    virtual void on_request_complete(
        int result, char type, int count, int ttl, const void* addresses) = 0;
    static void dispatch_on_request_complete(
        int result, char type, int count, int ttl, void* addresses, void* arg);

    DNSBase& dns_base;
    bool complete;
    const void* target;
    int flags;
    std::experimental::coroutine_handle<> coro;
  };

  class LookupIPv4Awaiter : public LookupAwaiterBase {
  public:
    using LookupAwaiterBase::LookupAwaiterBase;
    LookupResult<in_addr>&& await_resume();

  protected:
    LookupResult<in_addr> result;
    virtual void start_request();
    virtual void on_request_complete(
        int result, char type, int count, int ttl, const void* addresses);
  };

  class LookupIPv6Awaiter : public LookupAwaiterBase {
  public:
    using LookupAwaiterBase::LookupAwaiterBase;
    LookupResult<in6_addr>&& await_resume();

  protected:
    LookupResult<in6_addr> result;
    virtual void start_request();
    virtual void on_request_complete(
        int result, char type, int count, int ttl, const void* addresses);
  };

  class LookupReverseAwaiterBase : public LookupAwaiterBase {
  public:
    using LookupAwaiterBase::LookupAwaiterBase;
    LookupResult<std::string>&& await_resume();

  protected:
    LookupResult<std::string> result;
    virtual void on_request_complete(
        int result, char type, int count, int ttl, const void* addresses);
  };

  class LookupReverseIPv4Awaiter : public LookupReverseAwaiterBase {
  public:
    using LookupReverseAwaiterBase::LookupReverseAwaiterBase;
  protected:
    virtual void start_request();
  };

  class LookupReverseIPv6Awaiter : public LookupReverseAwaiterBase {
  public:
    using LookupReverseAwaiterBase::LookupReverseAwaiterBase;
  protected:
    virtual void start_request();
  };

  LookupIPv4Awaiter resolve_ipv4(const char* name, int flags = 0);
  LookupIPv6Awaiter resolve_ipv6(const char* name, int flags = 0);
  LookupReverseIPv4Awaiter resolve_reverse_ipv4(const struct in_addr* in,
      int flags = 0);
  LookupReverseIPv6Awaiter resolve_reverse_ipv6(const struct in6_addr* in,
      int flags = 0);

  static const char* err_to_string(int err);

  struct evdns_base* dns_base;
};

} // namespace EventAsync
