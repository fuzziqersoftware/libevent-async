#pragma once

#include <event2/event.h>
#include <event2/http.h>

#include <unordered_map>
#include <string>

#include "EvBuffer.hh"



struct HTTPRequest {
  HTTPRequest(EventBase& base);
  HTTPRequest(EventBase& base, struct evhttp_request* req);
  HTTPRequest(const HTTPRequest& req) = delete;
  HTTPRequest(HTTPRequest&& req);
  HTTPRequest& operator=(const HTTPRequest& req) = delete;
  HTTPRequest& operator=(HTTPRequest&& req) = delete;
  ~HTTPRequest();

  std::unordered_multimap<std::string, std::string> parse_url_params(
      const char* query = nullptr);
  std::unordered_map<std::string, std::string> parse_url_params_unique(
      const char* query = nullptr);

  enum evhttp_cmd_type get_command() const;
  struct evhttp_connection* get_connection();
  // TODO: We should write an HTTPURI class to wrap this
  const struct evhttp_uri* get_evhttp_uri() const;
  const char* get_host() const;

  EvBuffer get_input_buffer();
  EvBuffer get_output_buffer();

  struct evkeyvalq* get_input_headers();
  struct evkeyvalq* get_output_headers();
  const char* get_input_header(const char* header_name);
  void add_output_header(const char* header_name, const char* value);

  int get_response_code();
  const char* get_uri();

  EventBase& base;
  struct evhttp_request* req;
  bool owned;
};
