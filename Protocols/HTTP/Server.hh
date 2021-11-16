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

#include "../../Base.hh"
#include "../../Buffer.hh"
#include "../../Task.hh"
#include "Request.hh"



namespace EventAsync::HTTP {

class Server {
public:
  Server(Base& base, SSL_CTX* ssl_ctx);
  Server(const Server&) = delete;
  Server(Server&&) = delete;
  Server& operator=(const Server&) = delete;
  Server& operator=(Server&&) = delete;
  virtual ~Server();

  // Add a listening socket to this server. You should have already called
  // listen() on fd and made it nonblocking. If ssl is true, the server will
  // handle all connections on this fd over SSL. The same Server object can
  // serve SSL and non-SSL traffic by adding sockets multiple sockets here.
  void add_socket(int fd, bool ssl = false);

  // Sets the server name. If this is called, the server will automatically add
  // the Server response header to all subsequent responses.
  void set_server_name(const char* server_name);

protected:
  Base& base;
  struct evhttp* http;
  struct evhttp* ssl_http;
  SSL_CTX* ssl_ctx;
  std::string server_name;

  static struct bufferevent* dispatch_on_ssl_connection(
      struct event_base* base,
      void* ctx);
  static void dispatch_handle_request(struct evhttp_request* req, void* ctx);

  // When subclassing Server, you must implement this function to respond to
  // requests. Your implementation must either call one of the send_response
  // functions above or convert the request into a Websocket stream.
  virtual DetachedTask handle_request(Request& req) = 0;

  // Your handle_request implementation should call one of these functions to
  // send a response to the request. Failure to do so will result in a memory
  // leak.
  void send_response(
      Request& req,
      int code,
      const char* content_type,
      Buffer& b);
  void send_response(
      Request& req,
      int code,
      const char* content_type,
      const char* fmt, ...);
  void send_response(
      Request& req,
      int code,
      const char* content_type = nullptr);

  class WebsocketClient {
  public:
    WebsocketClient(Server* server, int fd);
    WebsocketClient(const WebsocketClient&) = delete;
    WebsocketClient(WebsocketClient&&);
    WebsocketClient& operator=(const WebsocketClient&) = delete;
    WebsocketClient& operator=(WebsocketClient&&);
    ~WebsocketClient();

    struct WebsocketMessage {
      uint8_t opcode;
      std::string data;
      WebsocketMessage();
    };

    // Waits for a returns a complete Websocket message from the client.
    Task<WebsocketMessage> read();

    // Sends a Websocket message to the client.
    Task<void> write(Buffer& buf, uint8_t opcode = 0x01);
    Task<void> write(const std::string& data, uint8_t opcode = 0x01);
    Task<void> write(const void* data, size_t size, uint8_t opcode = 0x01);

  protected:
    Server* server;
    int fd;

    static std::string encode_websocket_message_header(size_t data_size,
        uint8_t opcode);
  };

  // Converts the current request into a Websocket stream. Returns a
  // WebsocketClient object which you can use to send and receive websocket
  // messages. If this function returns nullptr then the request wasn't a
  // websocket request, or it failed to change protocols for some reason, and
  // you should still call send_response as for a normal HTTP request.
  Task<std::shared_ptr<WebsocketClient>> enable_websockets(Request& req);

  static const std::unordered_map<int, const char*> explanation_for_response_code;
};

} // namespace EventAsync::HTTP
