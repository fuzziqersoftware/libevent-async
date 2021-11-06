#pragma once

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent_ssl.h>
#include <event2/http.h>
#include <inttypes.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>

#include "HTTPServer.hh"



class HTTPWebsocketServer : public HTTPServer {
public:
  HTTPWebsocketServer(EventBase& base, SSL_CTX* ssl_ctx);
  HTTPWebsocketServer(const HTTPWebsocketServer&) = delete;
  HTTPWebsocketServer(HTTPWebsocketServer&&) = delete;
  HTTPWebsocketServer& operator=(const HTTPWebsocketServer&) = delete;
  HTTPWebsocketServer& operator=(HTTPWebsocketServer&&) = delete;
  virtual ~HTTPWebsocketServer() = default;

protected:
  class WebsocketClient {
  public:
    WebsocketClient(HTTPWebsocketServer* server, int fd);
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

    AsyncTask<WebsocketMessage> read();
    AsyncTask<void> write(EvBuffer& buf, uint8_t opcode = 0x01);
    AsyncTask<void> write(const std::string& data, uint8_t opcode = 0x01);
    AsyncTask<void> write(const void* data, size_t size, uint8_t opcode = 0x01);

  protected:
    HTTPWebsocketServer* server;
    int fd;

    static std::string encode_websocket_message_header(size_t data_size,
        uint8_t opcode);
  };

  AsyncTask<std::shared_ptr<WebsocketClient>> enable_websockets(
      HTTPServerRequest& req);
};
