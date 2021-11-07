#include <unordered_set>
#include <experimental/coroutine>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>

#include "HTTPConnection.hh"
#include "HTTPRequest.hh"
#include "EvDNSBase.hh"

using namespace std;



DetachedTask make_request(
    EventBase& base,
    const char* host,
    uint16_t port,
    const char* path) {

  shared_ptr<SSL_CTX> ssl_ctx;
  if (port == 443) {
    ssl_ctx.reset(HTTPConnection::create_default_ssl_ctx(), SSL_CTX_free);
  }
  EvDNSBase dns_base(base);
  HTTPConnection conn(base, dns_base, host, port, ssl_ctx.get());
  HTTPRequest req(base);
  req.add_output_header("Connection", "close");

  string request_path = escape_url(path, false);
  co_await conn.send_request(req, EVHTTP_REQ_GET, request_path.c_str());

  fprintf(stderr, "Response code: %d\n", req.get_response_code());
  fprintf(stderr, "Data:\n");
  req.get_input_buffer().debug_print_contents(stderr);
}

int main(int argc, char** argv) {
  if (argc != 4) {
    throw invalid_argument("Usage: HTTPClientExample hostname port request_path");
  }

  EventBase base;
  make_request(base, argv[1], atoi(argv[2]), argv[3]);
  base.run();
  return 0;
}
