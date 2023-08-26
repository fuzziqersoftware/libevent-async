#include <unistd.h>

#include <coroutine>
#include <phosg/Filesystem.hh>
#include <phosg/Network.hh>

#include "../Base.hh"
#include "../Buffer.hh"
#include "../Event.hh"
#include "../Task.hh"

using namespace std;

EventAsync::DetachedTask run(EventAsync::Base& base, int port) {
  scoped_fd fd = listen("", port, 0, true);
  for (;;) {
    auto recv_result = co_await base.recvfrom(fd, 0x1000);
    sendto(
        fd,
        recv_result.data.data(),
        recv_result.data.size(),
        0,
        reinterpret_cast<const sockaddr*>(&recv_result.addr),
        sizeof(recv_result.addr));
  }
}

int main(int, char**) {
  EventAsync::Base base;
  run(base, 5050);
  base.run();
  return 0;
}
