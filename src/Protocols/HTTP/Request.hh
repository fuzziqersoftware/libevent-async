#pragma once

#include <event2/event.h>
#include <event2/http.h>

#include <string>
#include <unordered_map>

#include "../../Buffer.hh"

namespace EventAsync::HTTP {

struct Request {
  Request(Base& base);
  Request(Base& base, struct evhttp_request* req);
  Request(const Request& req) = delete;
  Request(Request&& req);
  Request& operator=(const Request& req) = delete;
  Request& operator=(Request&& req) = delete;
  ~Request();

  static std::unordered_multimap<std::string, std::string> parse_url_params(
      const char* query);
  static std::unordered_map<std::string, std::string> parse_url_params_unique(
      const char* query);

  std::unordered_multimap<std::string, std::string> parse_url_params();
  std::unordered_map<std::string, std::string> parse_url_params_unique();

  enum evhttp_cmd_type get_command() const;
  struct evhttp_connection* get_connection();
  // TODO: We should write a URI class to wrap this
  const struct evhttp_uri* get_evhttp_uri() const;
  const char* get_host() const;

  Buffer get_input_buffer();
  Buffer get_output_buffer();

  struct evkeyvalq* get_input_headers();
  struct evkeyvalq* get_output_headers();
  const char* get_input_header(const char* header_name);
  void add_output_header(const char* header_name, const char* value);

  int get_response_code();
  const char* get_uri();

  Base& base;
  struct evhttp_request* req;
  bool owned;

  // This is unfortunate. There's no externally-available way to change a
  // request's cb or cb_arg after it's created, so we have to store context in
  // this class because the awaiter doesn't exist yet when the request object is
  // created.
  bool is_complete; // only relevant for outbound requests
  void* awaiter;

  static void on_response(struct evhttp_request* req, void* ctx);
};

} // namespace EventAsync::HTTP
