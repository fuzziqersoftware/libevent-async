#include "Request.hh"

#include <phosg/Strings.hh>
#include <phosg/Time.hh>

#include "Connection.hh"

using namespace std;

namespace EventAsync::HTTP {

Request::Request(Base& base)
    : base(base),
      req(evhttp_request_new(&Request::on_response, this)),
      owned(false),
      is_complete(false),
      awaiter(nullptr) {
  if (!this->req) {
    throw bad_alloc();
  }
}

Request::Request(Base& base, struct evhttp_request* req)
    : base(base),
      req(req),
      owned(false) {}

Request::Request(Request&& other)
    : base(other.base),
      req(other.req),
      owned(other.owned) {
  other.owned = false;
}

Request::~Request() {
  if (this->owned && this->req) {
    evhttp_request_free(this->req);
  }
}

unordered_multimap<string, string> Request::parse_url_params() {
  const struct evhttp_uri* uri = evhttp_request_get_evhttp_uri(this->req);
  const char* query = evhttp_uri_get_query(uri);
  return this->parse_url_params(query);
}

unordered_multimap<string, string> Request::parse_url_params(const char* query) {
  unordered_multimap<string, string> params;
  if (*query == '\0') {
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

unordered_map<string, string> Request::parse_url_params_unique() {
  const struct evhttp_uri* uri = evhttp_request_get_evhttp_uri(this->req);
  const char* query = evhttp_uri_get_query(uri);
  return this->parse_url_params_unique(query);
}

unordered_map<string, string> Request::parse_url_params_unique(
    const char* query) {
  unordered_map<string, string> ret;
  for (const auto& it : Request::parse_url_params(query)) {
    ret.emplace(it.first, std::move(it.second));
  }
  return ret;
}

enum evhttp_cmd_type Request::get_command() const {
  return evhttp_request_get_command(this->req);
}

struct evhttp_connection* Request::get_connection() {
  return evhttp_request_get_connection(this->req);
}

const struct evhttp_uri* Request::get_evhttp_uri() const {
  return evhttp_request_get_evhttp_uri(this->req);
}

const char* Request::get_host() const {
  return evhttp_request_get_host(this->req);
}

Buffer Request::get_input_buffer() {
  return Buffer(this->base, evhttp_request_get_input_buffer(this->req));
}

Buffer Request::get_output_buffer() {
  return Buffer(this->base, evhttp_request_get_output_buffer(this->req));
}

struct evkeyvalq* Request::get_input_headers() {
  return evhttp_request_get_input_headers(this->req);
}

struct evkeyvalq* Request::get_output_headers() {
  return evhttp_request_get_output_headers(this->req);
}

const char* Request::get_input_header(const char* header_name) {
  struct evkeyvalq* in_headers = this->get_input_headers();
  return evhttp_find_header(in_headers, header_name);
}

void Request::add_output_header(const char* header_name, const char* value) {
  struct evkeyvalq* out_headers = this->get_output_headers();
  evhttp_add_header(out_headers, header_name, value);
}

int Request::get_response_code() {
  return evhttp_request_get_response_code(this->req);
}

const char* Request::get_uri() {
  return evhttp_request_get_uri(this->req);
}

void Request::on_response(struct evhttp_request*, void* ctx) {
  auto* req = reinterpret_cast<Request*>(ctx);

  // By default, calling evhttp_make_request causes the request to become owned
  // by the connection object. We don't want that here - the caller is a
  // coroutine, and will need to examine the result after this callback returns.
  // Fortunately, libevent allows us to override the default ownership behavior.
  evhttp_request_own(req->req);

  req->is_complete = true;
  if (req->awaiter) {
    auto* aw = reinterpret_cast<Connection::Awaiter*>(req->awaiter);
    aw->on_response();
  }
}

} // namespace EventAsync::HTTP
