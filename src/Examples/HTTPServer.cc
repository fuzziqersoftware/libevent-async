#include <coroutine>
#include <phosg/Network.hh>
#include <unordered_set>

#include "../Protocols/HTTP/Server.hh"

using namespace std;

class ExampleHTTPServer : public EventAsync::HTTP::Server {
public:
  ExampleHTTPServer(EventAsync::Base& base) : Server(base, nullptr) {}

protected:
  virtual EventAsync::DetachedTask handle_request(
      EventAsync::HTTP::Request& req) {
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

int main(int, char**) {
  EventAsync::Base base;
  ExampleHTTPServer server(base);
  server.add_socket(listen("", 5050, SOMAXCONN));
  base.run();
  return 0;
}
