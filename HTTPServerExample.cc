#include <unordered_set>
#include <experimental/coroutine>
#include <phosg/Network.hh>

#include "HTTPServer.hh"

using namespace std;



class ExampleHTTPServer : public HTTPServer {
public:
  ExampleHTTPServer(EventBase& base) : HTTPServer(base, nullptr) { }

protected:
  virtual DetachedTask handle_request(HTTPServerRequest& req) {
    const auto* uri = req.get_evhttp_uri();
    this->send_response(
        req,
        200,
        "text/plain",
        "Looks like it works!\nYou requested: %s",
        evhttp_uri_get_path(uri));
    co_return;
  }
};

int main(int argc, char** argv) {
  EventBase base;
  ExampleHTTPServer server(base);
  server.add_socket(listen("", 5050, SOMAXCONN));
  base.run();
  return 0;
}
