#pragma once

#include <event2/event.h>
#include <event2/http.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <unordered_map>
#include <string>

#include "EvBuffer.hh"
#include "EventBase.hh"
#include "EvDNSBase.hh"
#include "HTTPRequest.hh"



struct HTTPConnection {
  HTTPConnection(
      EventBase& base,
      EvDNSBase& dns_base,
      const std::string& host,
      uint16_t port,
      SSL_CTX* ssl_ctx = nullptr);
  HTTPConnection(const HTTPConnection& req) = delete;
  HTTPConnection(HTTPConnection&& req);
  HTTPConnection& operator=(const HTTPConnection& req) = delete;
  HTTPConnection& operator=(HTTPConnection&& req) = delete;
  ~HTTPConnection();

  std::pair<std::string, uint16_t> get_peer() const;
  void set_local_address(const char* addr);
  void set_local_port(uint16_t port);
  void set_max_body_size(ev_ssize_t max_body_size);
  void set_max_headers_size(ev_ssize_t max_headers_size); 
  void set_retries(int retry_max);  
  void set_timeout(int timeout_secs);

  class Awaiter {
  public:
    Awaiter(HTTPRequest& req);
    bool await_ready() const noexcept;
    void await_suspend(std::experimental::coroutine_handle<> coro);
    void await_resume();
  private:
    static void on_response(struct evhttp_request *, void *);
    HTTPRequest& req;
    std::experimental::coroutine_handle<> coro;
  };

  Awaiter send_request(
      HTTPRequest& req,
      evhttp_cmd_type method,
      const char* path_and_query);

  EventBase& base;
  struct evhttp_connection* conn;
};
