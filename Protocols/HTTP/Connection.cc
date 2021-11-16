#include "Connection.hh"

#include <openssl/x509v3.h>
#include <event2/bufferevent_ssl.h>

#include <phosg/Strings.hh>
#include <phosg/Time.hh>

using namespace std;
using namespace std::experimental;



namespace EventAsync::HTTP {

Connection::Connection(
      Base& base,
      DNSBase& dns_base,
      const string& host,
      uint16_t port,
      SSL_CTX* ssl_ctx)
  : base(base) {

  if (ssl_ctx) {
    SSL* ssl = SSL_new(ssl_ctx);
    if (!ssl) {
      throw runtime_error("failed to create connection-specific ssl context");
    }

    X509_VERIFY_PARAM* param = SSL_get0_param(ssl);
    X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (!X509_VERIFY_PARAM_set1_host(param, host.data(), host.size())) {
      SSL_free(ssl);
      throw runtime_error("failed to set expected hostname");
    }

    SSL_set_verify(ssl, SSL_VERIFY_PEER, nullptr);

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
    SSL_set_tlsext_host_name(ssl, host.c_str());
#endif

    // bev takes ownership of ssl
    struct bufferevent* bev = bufferevent_openssl_socket_new(
        this->base.base,
        -1, // fd
        ssl,
        BUFFEREVENT_SSL_CONNECTING,
        BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
    if (!bev) {
      SSL_free(ssl);
      throw runtime_error("failed to create ssl bufferevent");
    }
    bufferevent_openssl_set_allow_dirty_shutdown(bev, 1);

    // conn takes ownership of bev
    this->conn = evhttp_connection_base_bufferevent_new(
        this->base.base,
        dns_base.dns_base,
        bev,
        host.c_str(),
        port);
    if (!conn) {
      bufferevent_free(bev);
      throw runtime_error("failed to create http connection");
    }
  } else {
    this->conn = evhttp_connection_base_new(
        this->base.base, dns_base.dns_base, host.c_str(), port);
    if (!this->conn) {
      throw runtime_error("failed to create http connection");
    }
  }
}

Connection::Connection(Connection&& other)
  : base(other.base), conn(other.conn) {
}

Connection::~Connection() {
  if (this->conn) {
    evhttp_connection_free(this->conn);
  }
}

SSL_CTX* Connection::create_default_ssl_ctx() {
  SSL_CTX* ssl_ctx = SSL_CTX_new(TLS_client_method());
  if (!ssl_ctx) {
    throw runtime_error("SSL_CTX_new did not succeed");
  }

  auto* store = SSL_CTX_get_cert_store(ssl_ctx);
  if (X509_STORE_set_default_paths(store) != 1) {
    throw runtime_error("X509_STORE_set_default_paths did not succeed");
  }

  SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);
  return ssl_ctx;
}

pair<string, uint16_t> Connection::get_peer() const {
  char* address = nullptr;
  uint16_t port;
  evhttp_connection_get_peer(this->conn, &address, &port);
  if (address == nullptr) {
    throw runtime_error("no peer on http connection");
  }
  return make_pair(address, port);
}

void Connection::set_local_address(const char* addr) {
  evhttp_connection_set_local_address(this->conn, addr);
}

void Connection::set_local_port(uint16_t port) {
  evhttp_connection_set_local_port(this->conn, port);
}

void Connection::set_max_body_size(ev_ssize_t max_body_size) {
  evhttp_connection_set_max_body_size(this->conn, max_body_size);
}

void Connection::set_max_headers_size(ev_ssize_t max_headers_size) {
  evhttp_connection_set_max_headers_size(this->conn, max_headers_size);
}

void Connection::set_retries(int retry_max) {
  evhttp_connection_set_retries(this->conn, retry_max);
}

void Connection::set_timeout(int timeout_secs) {
  evhttp_connection_set_timeout(this->conn, timeout_secs);
}



Connection::Awaiter::Awaiter(Request& req) : req(req), coro(nullptr) { }

bool Connection::Awaiter::await_ready() const noexcept {
  return this->req.is_complete;
}

void Connection::Awaiter::await_suspend(coroutine_handle<> coro) {
  this->req.awaiter = this;
  this->coro = coro;
}

void Connection::Awaiter::await_resume() {
}

void Connection::Awaiter::on_response() {
  this->coro.resume();
}

Connection::Awaiter Connection::send_request(
    Request& req,
    evhttp_cmd_type method,
    const char* path_and_query) {
  if (req.is_complete) {
    throw logic_error("attempted to re-send completed request");
  }
  if (evhttp_make_request(conn, req.req, method, path_and_query)) {
    throw runtime_error("failed to send http request");
  }
  return Awaiter(req);
}

} // namespace EventAsync::HTTP
