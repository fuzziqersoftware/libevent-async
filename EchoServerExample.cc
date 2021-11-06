#include <unordered_set>
#include <experimental/coroutine>
#include <phosg/Network.hh>

#include "AsyncTask.hh"
#include "EventBase.hh"
#include "Event.hh"
#include "EvBuffer.hh"
#include "Listener.hh"

using namespace std;



DetachedTask handle_server_connection(EventBase& base, int fd) {
  EvBuffer buf(base);
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

DetachedTask run_server(EventBase& base, int port) {
  Listener l(base, listen("", port, SOMAXCONN, true));
  for (;;) {
    int fd = co_await l.accept();
    handle_server_connection(base, fd);
  }
}

int main(int argc, char** argv) {
  EventBase base;
  run_server(base, 5050);
  base.run();
  return 0;
}
