#include <unistd.h>

#include <experimental/coroutine>
#include <phosg/Network.hh>

#include "Task.hh"
#include "Base.hh"
#include "Event.hh"
#include "Buffer.hh"

using namespace std;



EventAsync::DetachedTask handle_server_connection(EventAsync::Base& base, int fd) {
  EventAsync::Buffer buf(base);
  for (;;) {
    co_await buf.read_atmost(fd, 0x400);
    if (buf.get_length() == 0) {
      break; // EOF
    }
    co_await buf.write(fd);
    buf.drain_all();
  }
  close(fd);
}

EventAsync::DetachedTask run_server(EventAsync::Base& base, int port) {
  int listen_fd = listen("", port, SOMAXCONN, true);
  for (;;) {
    int fd = co_await base.accept(listen_fd);
    handle_server_connection(base, fd);
  }
}

int main(int argc, char** argv) {
  EventAsync::Base base;
  run_server(base, 5050);
  base.run();
  return 0;
}
