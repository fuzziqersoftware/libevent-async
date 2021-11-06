#include "HTTPWebsocketServer.hh"

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent_ssl.h>
#include <event2/http.h>
#include <inttypes.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <phosg/Encoding.hh>
#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Time.hh>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>


using namespace std;



HTTPWebsocketServer::HTTPWebsocketServer(EventBase& base, SSL_CTX* ssl_ctx)
  : HTTPServer(base, ssl_ctx) { }

HTTPWebsocketServer::WebsocketClient::WebsocketClient(
    HTTPWebsocketServer* server,
    int fd)
  : server(server),
    fd(fd) { }

HTTPWebsocketServer::WebsocketClient::WebsocketClient(WebsocketClient&& other)
  : server(other.server),
    fd(other.fd) {
  other.fd = -1;
}

HTTPWebsocketServer::WebsocketClient&
HTTPWebsocketServer::WebsocketClient::operator=(WebsocketClient&& other) {
  this->server = other.server;
  this->fd = other.fd;
  other.fd = -1;
  return *this;
}

HTTPWebsocketServer::WebsocketClient::~WebsocketClient() {
  if (this->fd >= 0) {
    close(this->fd);
  }
}

AsyncTask<shared_ptr<HTTPWebsocketServer::WebsocketClient>>
HTTPWebsocketServer::enable_websockets(HTTPRequest& req) {
  if (req.get_command() != EVHTTP_REQ_GET) {
    co_return nullptr;
  }

  struct evkeyvalq* in_headers = req.get_input_headers();
  const char* connection_header = evhttp_find_header(in_headers, "Connection");
  if (!connection_header || strcasecmp(connection_header, "upgrade")) {
    co_return nullptr;
  }

  const char* upgrade_header = evhttp_find_header(in_headers, "Upgrade");
  if (!upgrade_header || strcasecmp(upgrade_header, "websocket")) {
    co_return nullptr;
  }

  const char* sec_websocket_key = evhttp_find_header(in_headers, "Sec-WebSocket-Key");
  if (!sec_websocket_key) {
    co_return nullptr;
  }

  string sec_websocket_accept_data = sec_websocket_key;
  sec_websocket_accept_data += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  string sec_websocket_accept = base64_encode(sha1(sec_websocket_accept_data));

  // Make a copy of the fd, since there doesn't appear to be a way to modify the
  // bufferevent flags (to remove BEV_OPT_CLOSE_ON_FREE) after creation time.
  struct evhttp_connection* conn = req.get_connection();
  struct bufferevent* bev = evhttp_connection_get_bufferevent(conn);
  int fd = dup(bufferevent_getfd(bev));
  evhttp_connection_free(conn);

  // Send the HTTP reply, which enables websockets on the client side. All
  // communication after this will use the websocket protocol.
  EvBuffer buf(this->base);
  buf.add_printf("HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
\r\n", sec_websocket_accept.c_str());
  co_await buf.write(fd);

  co_return shared_ptr<WebsocketClient>(new WebsocketClient(this, fd));
}

HTTPWebsocketServer::WebsocketClient::WebsocketMessage::WebsocketMessage()
  : opcode(0) { }

AsyncTask<HTTPWebsocketServer::WebsocketClient::WebsocketMessage>
HTTPWebsocketServer::WebsocketClient::read() {
  auto& base = this->server->base;

  // TODO: We should use evbuffers here to avoid making lots of small read()
  // syscalls. We also should improve the evbuffer async API so this
  // implementation won't be cumbersome.

  WebsocketMessage msg;

  // A message may be fragmented into multiple frames, so we may need to receive
  // more than one frame. We also automatically respond to control messages
  // appropriately without returning to the calling coroutine.
  for (;;) {
    // We need at most 10 bytes to determine if there's a valid frame, or as
    // little as 2.
    uint8_t header_data[10];
    co_await base.read(this->fd, header_data, 2);

    // figure out the payload size
    bool has_mask = header_data[0] & 0x80;
    size_t header_size = 2;
    size_t payload_size = header_data[1] & 0x7F;
    if (payload_size == 0x7F) {
      co_await base.read(this->fd, &header_data[2], 8);
      payload_size = bswap64(*reinterpret_cast<const uint64_t*>(&header_data[2]));
      header_size = 10;
    } else if (payload_size == 0x7E) {
      co_await base.read(this->fd, &header_data[2], 2);
      payload_size = bswap16(*reinterpret_cast<const uint16_t*>(&header_data[2]));
      header_size = 4;
    }

    // Read the masking key if needed
    uint8_t mask_key[4];
    if (has_mask) {
      co_await base.read(this->fd, mask_key, 4);
    }

    // Read the message data and unmask it
    string frame_payload = co_await base.read(this->fd, payload_size);
    if (has_mask) {
      for (size_t x = 0; x < payload_size; x++) {
        frame_payload[x] ^= mask_key[x & 3];
      }
    }

    // If the current message is a control message, respond appropriately. Note
    // that control messages can be sent in the middle of fragmented messages;
    // we should not fail if that happens.
    uint8_t opcode = header_data[0] & 0x0F;
    if (opcode & 0x08) {
      if (opcode == 0x0A) {
        // Ignore ping responses
      } else if (opcode == 0x08) { // Quit
        co_await this->write(frame_payload, 0x08);
        throw runtime_error("client has disconnected");
      } else if (opcode == 0x09) { // Ping
        co_await this->write(frame_payload, 0x0A);
      } else { // Unrecognized control message
        throw runtime_error("unrecognized control message");
      }

    } else { // Not a control message
      // If this is the first frame in a message, the frame's opcode must not be
      // zero; if it's a continuation frame, the frame's opcode must be zero.
      if ((msg.opcode == 0) == (opcode == 0)) {
        throw runtime_error("invalid opcode");
      }

      // Save the message opcode, if present, and append the frame data
      if (opcode) {
        msg.opcode = opcode;
      }
      msg.data += frame_payload;

      // If the FIN bit is set, then the message is complete; otherwise, we need
      // to receive at least one more frame to complete the message.
      if (header_data[0] & 0x80) {
        co_return msg;
      }
    }
  }
}

string HTTPWebsocketServer::WebsocketClient::encode_websocket_message_header(
    size_t data_size, uint8_t opcode) {
  string header;
  header.push_back(0x80 | (opcode & 0x0F));
  if (data_size > 65535) {
    header.push_back(0x7F);
    header.resize(10);
    *reinterpret_cast<uint64_t*>(const_cast<char*>(header.data() + 2)) = bswap64(data_size);
  } else if (data_size > 0x7D) {
    header.push_back(0x7E);
    header.resize(4);
    *reinterpret_cast<uint16_t*>(const_cast<char*>(header.data() + 2)) = bswap16(data_size);
  } else {
    header.push_back(data_size);
  }
  return header;
}

AsyncTask<void> HTTPWebsocketServer::WebsocketClient::write(
    EvBuffer& buf, uint8_t opcode) {
  string header = this->encode_websocket_message_header(buf.get_length(), opcode);
  co_await this->server->base.write(this->fd, header);
  co_await buf.write(this->fd);
}

AsyncTask<void> HTTPWebsocketServer::WebsocketClient::write(
    const std::string& data, uint8_t opcode) {
  return this->write(data.data(), data.size(), opcode);
}

AsyncTask<void> HTTPWebsocketServer::WebsocketClient::write(
    const void* data, size_t size, uint8_t opcode) {
  string header = this->encode_websocket_message_header(size, opcode);
  co_await this->server->base.write(this->fd, header);
  co_await this->server->base.write(this->fd, data, size);
}
