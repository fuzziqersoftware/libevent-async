#pragma once

#include <event2/buffer.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
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
  Server(Base& base, std::shared_ptr<SSL_CTX> ssl_ctx);
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

  static SSL_CTX* create_server_ssl_ctx(
      const std::string& key_filename,
      const std::string& cert_filename,
      const std::string& ca_cert_filename);

protected:
  Base& base;
  struct evhttp* http;
  struct evhttp* ssl_http;
  std::shared_ptr<SSL_CTX> ssl_ctx;
  std::string server_name;

  static struct bufferevent* dispatch_on_ssl_connection(
      struct event_base* base,
      void* ctx);
  static void dispatch_handle_request(struct evhttp_request* req, void* ctx);

  // When subclassing Server, you must implement this function to respond to
  // requests. Your implementation must either call one of the send_response
  // functions below or convert the request into a Websocket stream. Failure to
  // do either of these will result in a memory leak.
  virtual DetachedTask handle_request(Request& req) = 0;

  // The send_response function sends a standard HTTP response to the client.
  // The code is an HTTP code (e.g. 200, 404, etc.). Most forms of this function
  // send a response with a body; the MIME type is specified by the content_type
  // argument. The last form sends a response with no body (hence content_type
  // is optional, and should usually be nullptr in this case).
  void send_response(
      Request& req,
      int code,
      const char* content_type,
      Buffer& b);
  void send_response(
      Request& req,
      int code,
      const char* content_type,
      const void* data,
      size_t size);
  void send_response(
      Request& req,
      int code,
      const char* content_type,
      std::string&& data);
  void send_response(
      Request& req,
      int code,
      const char* content_type,
      const char* fmt,
      ...);
  void send_response(
      Request& req,
      int code,
      const char* content_type = nullptr);

  class WebsocketClient {
  public:
    WebsocketClient(Server* server, struct evhttp_connection* conn);
    WebsocketClient(const WebsocketClient&) = delete;
    WebsocketClient(WebsocketClient&&);
    WebsocketClient& operator=(const WebsocketClient&) = delete;
    WebsocketClient& operator=(WebsocketClient&&);
    ~WebsocketClient();

    void close();
    bool is_closed() const;

    struct WebsocketMessage {
      uint8_t opcode;
      std::string data;
      WebsocketMessage();
    };

    // Waits for a returns a complete Websocket message from the client.
    Task<WebsocketMessage> read();

    // Sends a Websocket message to the client.
    Task<void> write(Buffer& buf, uint8_t opcode = 0x01);
    Task<void> write(const void* data, size_t size, uint8_t opcode = 0x01);

  protected:
    Server* server;
    struct evhttp_connection* conn;
    struct bufferevent* bev;
    int fd;
    Buffer input_buf;

    static std::string encode_websocket_message_header(size_t data_size,
        uint8_t opcode);
  };

  // Converts the current request into a Websocket stream. Returns a
  // WebsocketClient object which you can use to send and receive Websocket
  // messages. If you get nullptr instead of a client object, then the request
  // wasn't a Websocket request or it failed to change protocols for some
  // reason, and you should still call send_response as for a normal request.
  Task<std::shared_ptr<WebsocketClient>> enable_websockets(Request& req);

  static const std::unordered_map<int, const char*> explanation_for_response_code;
};

} // namespace EventAsync::HTTP
