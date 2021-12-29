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
#include <phosg/Strings.hh>
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



Server::Server(Base& base, shared_ptr<SSL_CTX> ssl_ctx)
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
      if (!this->ssl_ctx.get()) {
        throw logic_error("cannot add SSL listening socket without SSL_CTX set");
      }
      this->ssl_http = evhttp_new(this->base.base);
      if (!this->ssl_http) {
        throw bad_alloc();
      }
      evhttp_set_bevcb(
          this->ssl_http,
          this->dispatch_on_ssl_connection,
          this->ssl_ctx.get());
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

void Server::send_response(
    Request& req,
    int code,
    const char* content_type,
    Buffer& buf) {

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

void Server::send_response(
    Request& req,
    int code,
    const char* content_type,
    const void* data,
    size_t size) {
  Buffer buf(this->base);
  buf.add(data, size);
  this->send_response(req, code, content_type, buf);
}

void Server::send_response(
    Request& req,
    int code,
    const char* content_type,
    string&& data) {
  Buffer buf(this->base);
  buf.add(move(data));
  this->send_response(req, code, content_type, buf);
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



Server::WebsocketClient::WebsocketClient(
    Server* server, struct evhttp_connection* conn)
  : server(server),
    conn(conn),
    bev(evhttp_connection_get_bufferevent(this->conn)),
    fd(bufferevent_getfd(this->bev)),
    input_buf(server->base) { }

Server::WebsocketClient::WebsocketClient(WebsocketClient&& other)
  : server(other.server),
    conn(other.conn),
    bev(other.bev),
    fd(other.fd),
    input_buf(other.input_buf.base) {
  other.conn = nullptr;
  other.bev = nullptr;
  other.fd = -1;
  this->input_buf.add_buffer(other.input_buf);
}

Server::WebsocketClient& Server::WebsocketClient::operator=(
    WebsocketClient&& other) {
  this->server = other.server;
  this->conn = other.conn;
  this->bev = other.bev;
  this->fd = other.fd;
  this->input_buf.drain_all();
  this->input_buf.add_buffer(other.input_buf);
  other.conn = nullptr;
  other.bev = nullptr;
  other.fd = -1;
  return *this;
}

Server::WebsocketClient::~WebsocketClient() {
  // Assume the evhttp_connection (if present) owns the fd, so we only need to
  // free the conn or close the fd here (but not both).
  if (this->conn) {
    evhttp_connection_free(this->conn);
  } else if (this->fd >= 0) {
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

  struct evhttp_connection* conn = req.get_connection();
  struct bufferevent* bev = evhttp_connection_get_bufferevent(conn);
  int fd = bufferevent_getfd(bev);

  // Send the HTTP reply, which enables websockets on the client side. All
  // communication after this will use the websocket protocol.
  Buffer buf(this->base);
  buf.add_printf("HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: upgrade\r\n\
Sec-WebSocket-Accept: %s\r\n\
\r\n", sec_websocket_accept.c_str());
  co_await buf.write(fd);

  co_return shared_ptr<WebsocketClient>(new WebsocketClient(this, conn));
}

Server::WebsocketClient::WebsocketMessage::WebsocketMessage() : opcode(0) { }

Task<Server::WebsocketClient::WebsocketMessage>
Server::WebsocketClient::read() {
  WebsocketMessage msg;

  // A message may be fragmented into multiple frames, so we may need to receive
  // more than one frame. We also automatically respond to control messages
  // appropriately without returning to the calling coroutine.
  for (;;) {
    // We need at most 10 bytes to determine if there's a valid frame, or as
    // little as 2.
    co_await this->input_buf.read_to(this->fd, 2);
    uint8_t frame_opcode = this->input_buf.remove_u8();
    uint8_t frame_size = this->input_buf.remove_u8();

    // Figure out the payload size
    size_t payload_size = frame_size & 0x7F;
    if (payload_size == 0x7F) {
      co_await this->input_buf.read_to(this->fd, 8);
      payload_size = this->input_buf.remove_u64r();
    } else if (payload_size == 0x7E) {
      co_await this->input_buf.read_to(this->fd, 2);
      payload_size = this->input_buf.remove_u16r();
    }

    // Read the masking key if needed
    bool has_mask = frame_size & 0x80;
    uint8_t mask_key[4];
    if (has_mask) {
      co_await this->input_buf.read_to(this->fd, 4);
      this->input_buf.remove(mask_key, 4);
    }

    // Read the message data and unmask it
    co_await this->input_buf.read_to(this->fd, payload_size);
    string frame_payload = this->input_buf.remove(payload_size);
    if (has_mask) {
      for (size_t x = 0; x < payload_size; x++) {
        frame_payload[x] ^= mask_key[x & 3];
      }
    }

    // If the current message is a control message, respond appropriately. Note
    // that control messages can be sent in the middle of fragmented messages;
    // we should not fail if that happens.
    uint8_t opcode = frame_opcode & 0x0F;
    if (opcode & 0x08) {
      if (opcode == 0x0A) { // Ping response
        // (Ignore these)
      } else if (opcode == 0x08) { // Quit
        co_await this->write(frame_payload.data(), frame_payload.size(), 0x08);
        throw runtime_error("client has disconnected");
      } else if (opcode == 0x09) { // Ping
        co_await this->write(frame_payload.data(), frame_payload.size(), 0x0A);
      } else {
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
      if (frame_opcode & 0x80) {
        co_return msg;
      }
    }
  }
}

Task<void> Server::WebsocketClient::write(Buffer& buf, uint8_t opcode) {
  size_t data_size = buf.get_length();

  Buffer send_buf(this->server->base);
  // We don't fragment outgoing frames, so the FIN bit is always set here.
  send_buf.add_u8(0x80 | (opcode & 0x0F));
  if (data_size > 0xFFFF) {
    send_buf.add_u8(0x7F);
    send_buf.add_u64r(data_size);
  } else if (data_size > 0x7D) {
    send_buf.add_u8(0x7E);
    send_buf.add_u16r(data_size);
  } else {
    send_buf.add_u8(data_size);
  }
  send_buf.add_buffer(buf);

  co_await send_buf.write(this->fd);
}

Task<void> Server::WebsocketClient::write(
    const void* data, size_t size, uint8_t opcode) {
  Buffer buf(this->server->base);
  buf.add_reference(data, size);
  co_await this->write(buf, opcode);
}



SSL_CTX* Server::create_server_ssl_ctx(
    const string& key_filename,
    const string& cert_filename,
    const string& ca_cert_filename) {
  SSL_CTX* ssl_ctx = SSL_CTX_new(TLS_method());
  if (!ssl_ctx) {
    throw bad_alloc();
  }
  SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);
  SSL_CTX_set_cipher_list(ssl_ctx, "ECDH+AESGCM:ECDH+CHACHA20:DH+AESGCM:ECDH+AES256:DH+AES256:ECDH+AES128:DH+AES:RSA+AESGCM:RSA+AES:!aNULL:!MD5:!DSS:!AESCCM:!RSA");
  SSL_CTX_set_ecdh_auto(ssl_ctx, 1);
  SSL_CTX_load_verify_locations(ssl_ctx, ca_cert_filename.c_str(), nullptr);
  if (SSL_CTX_use_certificate_file(ssl_ctx, cert_filename.c_str(), SSL_FILETYPE_PEM) <= 0) {
    throw runtime_error("cannot load SSL certificate file " + cert_filename);
  }
  if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_filename.c_str(), SSL_FILETYPE_PEM) <= 0) {
    throw runtime_error("cannot load SSL private key " + key_filename);
  }
  return ssl_ctx;
}

} // namespace EventAsync::HTTP
