#include "HTTPServerRequest.hh"

#include <phosg/Strings.hh>

using namespace std;



HTTPConnection::HTTPConnection(
      EventBase& base,
      EvDNSBase& dns_base,
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

    SSL_set_verify(ssl, SSL_VERIFY_PEER, NULL);

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
    SSL_set_tlsext_host_name(ssl, this->hostname.c_str());
#endif

    // bev takes ownership of ssl
    struct bufferevent* bev = bufferevent_openssl_socket_new(
        this->base,
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
        this->dns_base.dns_base,
        bev,
        host.c_str(),
        port);
    if (!conn) {
      bufferevent_free(bev);
      throw runtime_error("failed to create http connection");
    }
  } else {
    this->conn = evhttp_connection_base_new(
        this->base, this->dns_base, host.c_str(), port);
    if (!this->conn) {
      throw runtime_error("failed to create http connection");
    }
  }
}

HTTPConnection::HTTPConnection(HTTPConnection&& other)
  : base(other.base), conn(other.conn) {
}

HTTPConnection::~HTTPConnection() {
  if (this->conn) {
    evhttp_connection_free(this->conn);
  }
}

std::pair<std::string, uint16_t> HTTPConnection::get_peer() const {
  char* address = nullptr;
  uint16_t port;
  evhttp_connection_get_peer(this->conn, &address, &port);
  if (address == nullptr) {
    throw runtime_error("no peer on http connection");
  }
  return make_pair(address, port);
}

void HTTPConnection::set_local_address(const char* addr) {
  evhttp_connection_set_local_address(this->conn, addr);
}

void HTTPConnection::set_local_port(uint16_t port) {
  evhttp_connection_set_local_port(this->conn, port);
}

void HTTPConnection::set_max_body_size(ev_ssize_t max_body_size) {
  evhttp_connection_set_max_body_size(this->conn, max_body_size);
}

void HTTPConnection::set_max_headers_size(ev_ssize_t max_headers_size) {
  evhttp_connection_set_max_headers_size(this->conn, max_headers_size);
}

void HTTPConnection::set_retries(int retry_max) {
  evhttp_connection_set_retries(this->conn, retry_max);
}

void HTTPConnection::set_timeout(int timeout_secs) {
  evhttp_connection_set_timeout(this->conn, timeout_secs);
}



HTTPConnection::Awaiter::Awaiter(HTTPRequest& req) : req(req), coro(nullptr) { }

bool HTTPConnection::Awaiter::await_ready() const noexcept {
  return false;
}

void HTTPConnection::Awaiter::await_suspend(std::experimental::coroutine_handle<> coro) {
  evhttp_request_set_on_complete_cb(this->req.req, &Awaiter::on_response, this);
}

void HTTPConnection::Awaiter::await_resume() { }

void HTTPConnection::Awaiter::on_response(struct evhttp_request* req, void* ctx) {
  // By default, calling evhttp_make_request causes the request to become owned
  // by the connection object. We don't want that here - the caller is a
  // coroutine, and will need to examine the result after this callback returns.
  // Fortunately, libevent allows us to override the default ownership behavior.
  evhttp_request_own(req);

  reinterpret_cast<Awaiter*>(ctx)->coro.resume();
}

HTTPConnection::Awaiter HTTPConnection::send_request(
    HTTPRequest& req,
    evhttp_cmd_type method,
    const char* path_and_query) {
  if (evhttp_make_request(conn, req, method, request_path.c_str())) {
    throw runtime_error("failed to send http request");
  }
  return Awaiter(req);
}