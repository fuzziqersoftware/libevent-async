#include <unordered_set>
#include <experimental/coroutine>
#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Network.hh>
#include <phosg/Strings.hh>

#include "../Protocols/HTTP/Server.hh"

using namespace std;



class ExampleHTTPWebsocketServer : public EventAsync::HTTP::Server {
public:
  ExampleHTTPWebsocketServer(EventAsync::Base& base) : Server(base, nullptr) { }

protected:
  virtual EventAsync::DetachedTask handle_request(
      EventAsync::HTTP::Request& req) {
    const char* path = req.get_uri();

    // redirect / to /static/index.html
    if (!strcmp(path, "/")) {
      path = "/static/index.html";
    }

    if (starts_with(path, "/static/")) {
      // TODO: make this not depend on the current directory (so e.g. running
      // this executable as Examples/HTPWebsocketServer doesn't break)
      string filename = "HTTPWebsocketServer-Static/";
      filename += &path[8];
      if (filename.find("..") != string::npos) {
        this->send_response(req, 400, "text/plain", "Invalid static file");

      } else {
        string data;
        try {
          data = load_file(filename);
        } catch (const exception& e) {
          this->send_response(
              req, 500, "text/plain", "Could not read file: %s", e.what());
          co_return;
        }

        const char* content_type = "text/html";
        if (ends_with(filename, ".js")) {
          content_type = "text/javascript";
        }
        this->send_response(req, 200, content_type, move(data));
      }

    } else if (starts_with(path, "/stream")) {
      auto c = co_await this->enable_websockets(req);
      if (!c.get()) {
        this->send_response(
            req,
            400,
            "text/plain",
            "Could not enable websockets for request");

      } else {
        // This websocket handler just replies with the rot13 encoding of
        // whatever the client sends, until they send "quit" - then it
        // disconnects them (by letting the WebsocketClient object get
        // destroyed).
        for (;;) {
          auto msg = co_await c->read();
          string response = rot13(msg.data.data(), msg.data.size());
          co_await c->write(response.data(), response.size());
          if (msg.data == "quit") {
            break;
          }
        }
      }

    } else {
      this->send_response(
          req,
          404,
          "text/plain",
          "Incorrect path: %s",
          path);
    }
  }
};

int main(int, char**) {
  signal(SIGPIPE, SIG_IGN);

  if (!isdir("HTTPWebsocketServer-Static")) {
    throw runtime_error("There must be an HTTPWebsocketServer-Static directory in the current directory for this program to work properly. Run it from within the Examples directory.");
  }

  EventAsync::Base base;
  ExampleHTTPWebsocketServer server(base);
  server.add_socket(listen("", 5050, SOMAXCONN));
  base.run();
  return 0;
}
