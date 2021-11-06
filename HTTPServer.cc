#include "HTTPServer.hh"

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent_ssl.h>
#include <event2/http.h>
#include <inttypes.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <phosg/Encoding.hh>
#include <string>
#include <thread>
#include <vector>

using namespace std;



const unordered_map<int, const char*> HTTPServer::explanation_for_response_code({
  {100, "Continue"},
  {101, "Switching Protocols"},
  {102, "Processing"},
  {200, "OK"},
  {201, "Created"},
  {202, "Accepted"},
  {203, "Non-Authoritative Information"},
  {204, "No Content"},
  {205, "Reset Content"},
  {206, "Partial Content"},
  {207, "Multi-Status"},
  {208, "Already Reported"},
  {226, "IM Used"},
  {300, "Multiple Choices"},
  {301, "Moved Permanently"},
  {302, "Found"},
  {303, "See Other"},
  {304, "Not Modified"},
  {305, "Use Proxy"},
  {307, "Temporary Redirect"},
  {308, "Permanent Redirect"},
  {400, "Bad Request"},
  {401, "Unathorized"},
  {402, "Payment Required"},
  {403, "Forbidden"},
  {404, "Not Found"},
  {405, "Method Not Allowed"},
  {406, "Not Acceptable"},
  {407, "Proxy Authentication Required"},
  {408, "Request Timeout"},
  {409, "Conflict"},
  {410, "Gone"},
  {411, "Length Required"},
  {412, "Precondition Failed"},
  {413, "Request Entity Too Large"},
  {414, "Request-URI Too Long"},
  {415, "Unsupported Media Type"},
  {416, "Requested Range Not Satisfiable"},
  {417, "Expectation Failed"},
  {418, "I\'m a Teapot"},
  {420, "Enhance Your Calm"},
  {422, "Unprocessable Entity"},
  {423, "Locked"},
  {424, "Failed Dependency"},
  {426, "Upgrade Required"},
  {428, "Precondition Required"},
  {429, "Too Many Requests"},
  {431, "Request Header Fields Too Large"},
  {444, "No Response"},
  {449, "Retry With"},
  {451, "Unavailable For Legal Reasons"},
  {500, "Internal Server Error"},
  {501, "Not Implemented"},
  {502, "Bad Gateway"},
  {503, "Service Unavailable"},
  {504, "Gateway Timeout"},
  {505, "HTTP Version Not Supported"},
  {506, "Variant Also Negotiates"},
  {507, "Insufficient Storage"},
  {508, "Loop Detected"},
  {509, "Bandwidth Limit Exceeded"},
  {510, "Not Extended"},
  {511, "Network Authentication Required"},
  {598, "Network Read Timeout Error"},
  {599, "Network Connect Timeout Error"},
});



HTTPServer::HTTPServer(EventBase& base, SSL_CTX* ssl_ctx)
  : base(base), http(nullptr), ssl_http(nullptr), ssl_ctx(ssl_ctx) { }

HTTPServer::~HTTPServer() {
  if (this->http) {
    evhttp_free(this->http);
  }
  if (this->ssl_http) {
    evhttp_free(this->ssl_http);
  }
}

void HTTPServer::add_socket(int fd, bool ssl) {
  if (ssl) {
    if (!this->ssl_http) {
      this->ssl_http = evhttp_new(this->base.base);
      evhttp_set_bevcb(
          this->ssl_http,
          this->dispatch_on_ssl_connection,
          this->ssl_ctx);
      evhttp_set_gencb(this->ssl_http, this->dispatch_handle_request, this);
    }
    evhttp_accept_socket(this->ssl_http, fd);

  } else {
    if (!this->http) {
      this->http = evhttp_new(this->base.base);
      evhttp_set_gencb(this->http, this->dispatch_handle_request, this);
    }
    evhttp_accept_socket(this->http, fd);
  }
}

void HTTPServer::set_server_name(const char* new_server_name) {
  this->server_name = new_server_name;
}

struct bufferevent* HTTPServer::dispatch_on_ssl_connection(
    struct event_base* base,
    void* ctx) {

  SSL_CTX* ssl_ctx = reinterpret_cast<SSL_CTX*>(ctx);
  SSL* ssl = SSL_new(ssl_ctx);
  return bufferevent_openssl_socket_new(
      base,
      -1,
      ssl,
      BUFFEREVENT_SSL_ACCEPTING,
      BEV_OPT_CLOSE_ON_FREE);
}

void HTTPServer::dispatch_handle_request(
    struct evhttp_request* req,
    void* ctx) {
  auto* s = reinterpret_cast<HTTPServer*>(ctx);
  HTTPServerRequest req_obj(s->base, req);
  s->handle_request(req_obj);
}

void HTTPServer::send_response(HTTPServerRequest& req, int code,
    const char* content_type, EvBuffer& buf) {

  req.add_output_header("Content-Type", content_type);
  if (!this->server_name.empty()) {
    req.add_output_header("Server", this->server_name.c_str());
  }

  evhttp_send_reply(
      req.req,
      code,
      HTTPServer::explanation_for_response_code.at(code),
      buf.buf);
}

void HTTPServer::send_response(HTTPServerRequest& req, int code,
    const char* content_type, const char* fmt, ...) {
  EvBuffer out_buffer(this->base);

  va_list va;
  va_start(va, fmt);
  out_buffer.add_vprintf(fmt, va);
  va_end(va);

  HTTPServer::send_response(req, code, content_type, out_buffer);
}

void HTTPServer::send_response(HTTPServerRequest& req, int code,
    const char* content_type) {
  if (!this->server_name.empty()) {
    req.add_output_header("Server", this->server_name.c_str());
  }
  if (content_type) {
    req.add_output_header("Content-Type", content_type);
  }
  evhttp_send_reply(
      req.req,
      code,
      HTTPServer::explanation_for_response_code.at(code),
      nullptr);
}
