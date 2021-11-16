#include <unordered_set>
#include <experimental/coroutine>
#include <phosg/Network.hh>

#include "Task.hh"
#include "Base.hh"
#include "Event.hh"
#include "Buffer.hh"
#include "Listener.hh"

using namespace std;



EventAsync::DetachedTask handle_server_connection(EventAsync::Base& base, int fd) {
  EventAsync::Buffer buf(base);
  for (;;) {
    co_await buf.read(fd, 0x400);
    if (buf.get_length() == 0) {
      break; // EOF
    }
    co_await buf.write(fd);
    buf.drain_all();
  }
  close(fd);
}

EventAsync::DetachedTask run_server(EventAsync::Base& base, int port) {
  EventAsync::Listener l(base, listen("", port, SOMAXCONN, true));
  for (;;) {
    int fd = co_await l.accept();
    handle_server_connection(base, fd);
  }
}

int main(int argc, char** argv) {
  EventAsync::Base base;
  run_server(base, 5050);
  base.run();
  return 0;
}
