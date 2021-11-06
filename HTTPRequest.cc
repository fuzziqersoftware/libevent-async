#include "HTTPRequest.hh"

#include <phosg/Strings.hh>

using namespace std;



HTTPRequest::HTTPRequest(EventBase& base)
  : base(base),
    req(evhttp_request_new(nullptr, nullptr)),
    owned(false) {
  if (!this->req) {
    throw bad_alloc();
  }
}

HTTPRequest::HTTPRequest(EventBase& base, struct evhttp_request* req)
  : base(base), req(req), owned(false) { }

HTTPRequest::HTTPRequest(HTTPRequest&& other)
  : base(other.base), req(other.req), owned(other.owned) {
  other.owned = false;
}

HTTPRequest::~HTTPRequest() {
  if (this->owned && this->req) {
    evhttp_request_free(this->req);
  }
}

unordered_multimap<string, string> HTTPRequest::parse_url_params(const char* query_str) {
  string query;
  if (query_str) {
    query = query_str;
  } else {
    const struct evhttp_uri* uri = evhttp_request_get_evhttp_uri(this->req);
    query = evhttp_uri_get_query(uri);
  }

  unordered_multimap<string, string> params;
  if (query.empty()) {
    return params;
  }
  for (auto it : split(query, '&')) {
    size_t first_equals = it.find('=');
    if (first_equals != string::npos) {
      string value(it, first_equals + 1);

      size_t write_offset = 0, read_offset = 0;
      for (; read_offset < value.size(); write_offset++) {
        if ((value[read_offset] == '%') && (read_offset < value.size() - 2)) {
          value[write_offset] =
              static_cast<char>(value_for_hex_char(value[read_offset + 1]) << 4) |
              static_cast<char>(value_for_hex_char(value[read_offset + 2]));
              read_offset += 3;
        } else if (value[write_offset] == '+') {
          value[write_offset] = ' ';
          read_offset++;
        } else {
          value[write_offset] = value[read_offset];
          read_offset++;
        }
      }
      value.resize(write_offset);

      params.emplace(piecewise_construct, forward_as_tuple(it, 0, first_equals),
          forward_as_tuple(value));
    } else {
      params.emplace(it, "");
    }
  }
  return params;
}

unordered_map<string, string> HTTPRequest::parse_url_params_unique(
    const char* query) {
  unordered_map<string, string> ret;
  for (const auto& it : HTTPRequest::parse_url_params(query)) {
    ret.emplace(it.first, move(it.second));
  }
  return ret;
}

enum evhttp_cmd_type HTTPRequest::get_command() const {
  return evhttp_request_get_command(this->req);
}

struct evhttp_connection* HTTPRequest::get_connection() {
  return evhttp_request_get_connection(this->req);
}

const struct evhttp_uri* HTTPRequest::get_evhttp_uri() const {
  return evhttp_request_get_evhttp_uri(this->req);
}

const char* HTTPRequest::get_host() const {
  return evhttp_request_get_host(this->req);
}

EvBuffer HTTPRequest::get_input_buffer() {
  return EvBuffer(this->base, evhttp_request_get_input_buffer(this->req));
}

EvBuffer HTTPRequest::get_output_buffer() {
  return EvBuffer(this->base, evhttp_request_get_output_buffer(this->req));
}

struct evkeyvalq* HTTPRequest::get_input_headers() {
  return evhttp_request_get_input_headers(this->req);
}

struct evkeyvalq* HTTPRequest::get_output_headers() {
  return evhttp_request_get_output_headers(this->req);
}

const char* HTTPRequest::get_input_header(const char* header_name) {
  struct evkeyvalq* in_headers = this->get_input_headers();
  return evhttp_find_header(in_headers, header_name);
}

void HTTPRequest::add_output_header(const char* header_name, const char* value) {
  struct evkeyvalq* out_headers = this->get_output_headers();
  evhttp_add_header(out_headers, header_name, value);
}

int HTTPRequest::get_response_code() {
  return evhttp_request_get_response_code(this->req);
}

const char* HTTPRequest::get_uri() {
  return evhttp_request_get_uri(this->req);
}
