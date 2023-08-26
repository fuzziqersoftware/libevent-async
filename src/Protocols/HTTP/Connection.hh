#pragma once

#include <event2/event.h>
#include <event2/http.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <string>
#include <unordered_map>

#include "../../Base.hh"
#include "../../Buffer.hh"
#include "../../DNSBase.hh"
#include "Request.hh"

namespace EventAsync::HTTP {

struct Connection {
  Connection(
      Base& base,
      DNSBase& dns_base,
      const std::string& host,
      uint16_t port,
      SSL_CTX* ssl_ctx = nullptr);
  Connection(const Connection& req) = delete;
  Connection(Connection&& req);
  Connection& operator=(const Connection& req) = delete;
  Connection& operator=(Connection&& req) = delete;
  ~Connection();

  static SSL_CTX* create_default_ssl_ctx();

  std::pair<std::string, uint16_t> get_peer() const;
  void set_local_address(const char* addr);
  void set_local_port(uint16_t port);
  void set_max_body_size(ev_ssize_t max_body_size);
  void set_max_headers_size(ev_ssize_t max_headers_size);
  void set_retries(int retry_max);
  void set_timeout(int timeout_secs);

  class Awaiter {
  public:
    Awaiter(Request& req);
    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> coro);
    void await_resume();
    void on_response();

  private:
    Request& req;
    std::coroutine_handle<> coro;
  };

  Awaiter send_request(
      Request& req,
      evhttp_cmd_type method,
      const char* path_and_query);

  Base& base;
  struct evhttp_connection* conn;
};

} // namespace EventAsync::HTTP
