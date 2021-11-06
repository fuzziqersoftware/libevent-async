#pragma once

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent_ssl.h>
#include <event2/http.h>
#include <inttypes.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>

#include "EventBase.hh"
#include "EvBuffer.hh"
#include "HTTPServerRequest.hh"
#include "AsyncTask.hh"



class HTTPServer {
public:
  HTTPServer(EventBase& base, SSL_CTX* ssl_ctx);
  HTTPServer(const HTTPServer&) = delete;
  HTTPServer(HTTPServer&&) = delete;
  HTTPServer& operator=(const HTTPServer&) = delete;
  HTTPServer& operator=(HTTPServer&&) = delete;
  virtual ~HTTPServer();

  void add_socket(int fd, bool ssl = false);

  void set_server_name(const char* server_name);

protected:
  EventBase& base;
  struct evhttp* http;
  struct evhttp* ssl_http;
  SSL_CTX* ssl_ctx;
  std::string server_name;

  static struct bufferevent* dispatch_on_ssl_connection(
      struct event_base* base,
      void* ctx);
  static void dispatch_handle_request(struct evhttp_request* req, void* ctx);

  void send_response(
      HTTPServerRequest& req,
      int code,
      const char* content_type,
      EvBuffer& b);
  void send_response(
      HTTPServerRequest& req,
      int code,
      const char* content_type,
      const char* fmt, ...);
  void send_response(
      HTTPServerRequest& req,
      int code,
      const char* content_type = nullptr);

  virtual DetachedTask handle_request(HTTPServerRequest& req) = 0;

  static const std::unordered_map<int, const char*> explanation_for_response_code;
};
