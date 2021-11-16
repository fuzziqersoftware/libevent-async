#include "Server.hh"

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
#include <unistd.h>

#include <phosg/Encoding.hh>
#include <phosg/Hash.hh>
#include <string>
#include <thread>
#include <vector>

using namespace std;



namespace EventAsync::HTTP {

const unordered_map<int, const char*> Server::explanation_for_response_code({
  {100, "Continue"},
  {101, "Switching Protocols"},
  {102, "Processing"},
  {200, "OK"},
  {201, "Created"},
  {202, "Accepted"},
  {203, "Non-Authoritative Information"},
  {204, "No Content"},
  {205, "Reset Content"},
  {206, "Partial Content"},
  {207, "Multi-Status"},
  {208, "Already Reported"},
  {226, "IM Used"},
  {300, "Multiple Choices"},
  {301, "Moved Permanently"},
  {302, "Found"},
  {303, "See Other"},
  {304, "Not Modified"},
  {305, "Use Proxy"},
  {307, "Temporary Redirect"},
  {308, "Permanent Redirect"},
  {400, "Bad Request"},
  {401, "Unathorized"},
  {402, "Payment Required"},
  {403, "Forbidden"},
  {404, "Not Found"},
  {405, "Method Not Allowed"},
  {406, "Not Acceptable"},
  {407, "Proxy Authentication Required"},
  {408, "Request Timeout"},
  {409, "Conflict"},
  {410, "Gone"},
  {411, "Length Required"},
  {412, "Precondition Failed"},
  {413, "Request Entity Too Large"},
  {414, "Request-URI Too Long"},
  {415, "Unsupported Media Type"},
  {416, "Requested Range Not Satisfiable"},
  {417, "Expectation Failed"},
  {418, "I\'m a Teapot"},
  {420, "Enhance Your Calm"},
  {422, "Unprocessable Entity"},
  {423, "Locked"},
  {424, "Failed Dependency"},
  {426, "Upgrade Required"},
  {428, "Precondition Required"},
  {429, "Too Many Requests"},
  {431, "Request Header Fields Too Large"},
  {444, "No Response"},
  {449, "Retry With"},
  {451, "Unavailable For Legal Reasons"},
  {500, "Internal Server Error"},
  {501, "Not Implemented"},
  {502, "Bad Gateway"},
  {503, "Service Unavailable"},
  {504, "Gateway Timeout"},
  {505, "HTTP Version Not Supported"},
  {506, "Variant Also Negotiates"},
  {507, "Insufficient Storage"},
  {508, "Loop Detected"},
  {509, "Bandwidth Limit Exceeded"},
  {510, "Not Extended"},
  {511, "Network Authentication Required"},
  {598, "Network Read Timeout Error"},
  {599, "Network Connect Timeout Error"},
});



Server::Server(Base& base, SSL_CTX* ssl_ctx)
  : base(base), http(nullptr), ssl_http(nullptr), ssl_ctx(ssl_ctx) { }

Server::~Server() {
  if (this->http) {
    evhttp_free(this->http);
  }
  if (this->ssl_http) {
    evhttp_free(this->ssl_http);
  }
}

void Server::add_socket(int fd, bool ssl) {
  if (ssl) {
    if (!this->ssl_http) {
      this->ssl_http = evhttp_new(this->base.base);
      if (!this->ssl_http) {
        throw bad_alloc();
      }
      evhttp_set_bevcb(
          this->ssl_http,
          this->dispatch_on_ssl_connection,
          this->ssl_ctx);
      evhttp_set_gencb(this->ssl_http, this->dispatch_handle_request, this);
    }
    evhttp_accept_socket(this->ssl_http, fd);

  } else {
    if (!this->http) {
      this->http = evhttp_new(this->base.base);
      if (!this->http) {
        throw bad_alloc();
      }
      evhttp_set_gencb(this->http, this->dispatch_handle_request, this);
    }
    evhttp_accept_socket(this->http, fd);
  }
}

void Server::set_server_name(const char* new_server_name) {
  this->server_name = new_server_name;
}

struct bufferevent* Server::dispatch_on_ssl_connection(
    struct event_base* base,
    void* ctx) {

  SSL_CTX* ssl_ctx = reinterpret_cast<SSL_CTX*>(ctx);
  SSL* ssl = SSL_new(ssl_ctx);
  return bufferevent_openssl_socket_new(
      base,
      -1,
      ssl,
      BUFFEREVENT_SSL_ACCEPTING,
      BEV_OPT_CLOSE_ON_FREE);
}

void Server::dispatch_handle_request(
    struct evhttp_request* req,
    void* ctx) {
  auto* s = reinterpret_cast<Server*>(ctx);
  Request req_obj(s->base, req);
  s->handle_request(req_obj);
}

void Server::send_response(Request& req, int code,
    const char* content_type, Buffer& buf) {

  req.add_output_header("Content-Type", content_type);
  if (!this->server_name.empty()) {
    req.add_output_header("Server", this->server_name.c_str());
  }

  evhttp_send_reply(
      req.req,
      code,
      Server::explanation_for_response_code.at(code),
      buf.buf);
}

void Server::send_response(Request& req, int code,
    const char* content_type, const char* fmt, ...) {
  Buffer out_buffer(this->base);

  va_list va;
  va_start(va, fmt);
  out_buffer.add_vprintf(fmt, va);
  va_end(va);

  Server::send_response(req, code, content_type, out_buffer);
}

void Server::send_response(Request& req, int code,
    const char* content_type) {
  if (!this->server_name.empty()) {
    req.add_output_header("Server", this->server_name.c_str());
  }
  if (content_type) {
    req.add_output_header("Content-Type", content_type);
  }
  evhttp_send_reply(
      req.req,
      code,
      Server::explanation_for_response_code.at(code),
      nullptr);
}



Server::WebsocketClient::WebsocketClient(Server* server, int fd)
  : server(server),
    fd(fd) { }

Server::WebsocketClient::WebsocketClient(WebsocketClient&& other)
  : server(other.server),
    fd(other.fd) {
  other.fd = -1;
}

Server::WebsocketClient& Server::WebsocketClient::operator=(
    WebsocketClient&& other) {
  this->server = other.server;
  this->fd = other.fd;
  other.fd = -1;
  return *this;
}

Server::WebsocketClient::~WebsocketClient() {
  if (this->fd >= 0) {
    close(this->fd);
  }
}

Task<shared_ptr<Server::WebsocketClient>> Server::enable_websockets(
    Request& req) {
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
  Buffer buf(this->base);
  buf.add_printf("HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
\r\n", sec_websocket_accept.c_str());
  co_await buf.write(fd);

  co_return shared_ptr<WebsocketClient>(new WebsocketClient(this, fd));
}

Server::WebsocketClient::WebsocketMessage::WebsocketMessage() : opcode(0) { }

Task<Server::WebsocketClient::WebsocketMessage>
Server::WebsocketClient::read() {
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

string Server::WebsocketClient::encode_websocket_message_header(
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

Task<void> Server::WebsocketClient::write(Buffer& buf, uint8_t opcode) {
  string header = this->encode_websocket_message_header(buf.get_length(), opcode);
  co_await this->server->base.write(this->fd, header);
  co_await buf.write(this->fd);
}

Task<void> Server::WebsocketClient::write(
    const std::string& data, uint8_t opcode) {
  return this->write(data.data(), data.size(), opcode);
}

Task<void> Server::WebsocketClient::write(
    const void* data, size_t size, uint8_t opcode) {
  string header = this->encode_websocket_message_header(size, opcode);
  co_await this->server->base.write(this->fd, header);
  co_await this->server->base.write(this->fd, data, size);
}

} // namespace EventAsync::HTTP
