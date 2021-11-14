#pragma once

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent_ssl.h>
#include <event2/http.h>
#include <inttypes.h>
#include <stdlib.h>

#include <string>
#include <unordered_map>

#include "Server.hh"



namespace EventAsync::HTTP {

class WebsocketServer : public Server {
public:
  WebsocketServer(EventBase& base, SSL_CTX* ssl_ctx);
  WebsocketServer(const WebsocketServer&) = delete;
  WebsocketServer(WebsocketServer&&) = delete;
  WebsocketServer& operator=(const WebsocketServer&) = delete;
  WebsocketServer& operator=(WebsocketServer&&) = delete;
  virtual ~WebsocketServer() = default;

protected:
  class WebsocketClient {
  public:
    WebsocketClient(WebsocketServer* server, int fd);
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

    Task<WebsocketMessage> read();
    Task<void> write(EvBuffer& buf, uint8_t opcode = 0x01);
    Task<void> write(const std::string& data, uint8_t opcode = 0x01);
    Task<void> write(const void* data, size_t size, uint8_t opcode = 0x01);

  protected:
    WebsocketServer* server;
    int fd;

    static std::string encode_websocket_message_header(size_t data_size,
        uint8_t opcode);
  };

  Task<std::shared_ptr<WebsocketClient>> enable_websockets(Request& req);
};

} // namespace EventAsync::HTTP
