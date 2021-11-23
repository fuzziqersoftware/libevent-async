#include <unordered_set>
#include <experimental/coroutine>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>

#include "../Protocols/HTTP/Connection.hh"
#include "../Protocols/HTTP/Request.hh"
#include "../DNSBase.hh"

using namespace std;



EventAsync::DetachedTask make_request(
    EventAsync::Base& base,
    const char* host,
    uint16_t port,
    const char* path) {

  shared_ptr<SSL_CTX> ssl_ctx;
  if (port == 443) {
    ssl_ctx.reset(
        EventAsync::HTTP::Connection::create_default_ssl_ctx(),
        SSL_CTX_free);
  }
  EventAsync::DNSBase dns_base(base);
  EventAsync::HTTP::Connection conn(base, dns_base, host, port, ssl_ctx.get());
  EventAsync::HTTP::Request req(base);
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

  EventAsync::Base base;
  make_request(base, argv[1], atoi(argv[2]), argv[3]);
  base.run();
  return 0;
}
