# libevent-async

libevent-async is a C++ wrapper around the libevent API. It wraps commonly-used objects in classes and provides coroutine support for making and accepting connections, reading and writing data, and waiting for timeouts.

This library was inspired by [rnburn's coevent](https://github.com/rnburn/coevent). This library takes a different approach in that it attempts to expose as much of libevent's functionality as possible using coroutines and other modern paradigms. (In a few places this gets kind of messy, unfortunately.)

## Useful stuff

* `AsyncTask<ReturnT>`: The common coroutine task type. Functions defined with this type are coroutines that co_return the specified type (which may be void). Execution does not begin until the task is co_awaited, and tasks are not destroyed automatically upon returning.
* `DetachedTask`: Used for tasks that execute independently of their callers. Before calling EventBase::run, call one or more DetachedTasks in order to create servers and whatnot. Unlike AsyncTasks, DetachedTasks begin executing immediately when they are called, and are automatically destroyed when their coroutine returns. They may not return a value.
* `EventBase`
  * `run`: Runs the event loop, just like event_base_dispatch().
  * `co_await EventBase::sleep`: Suspends the caller for the given time.
  * `co_await EventBase::read`: Reads data from a (nonblocking) file descriptor.
  * `co_await EventBase::write`: Writes data to a (nonblocking) file descriptor.
  * `co_await EventBase::connect`: Connects to a remote server.
* `Listener`
  * `co_await Listener::accept`: Waits for an incoming connection, and when one arrives, returns its fd.
* `EvBuffer`
  * `co_await EvBuffer::read`: Reads up to the given number of bytes and adds it to the buffer. This awaiter resumes when *any* nonzero amount of data is read, which may be less than the amount requested.
  * `co_await EvBuffer::read_exactly`: Reads the given number of bytes and adds it to the buffer. This awaiter *does not* resume until the requested number of bytes have been read.
  * `co_await EvBuffer::read_to`: Reads enough bytes such that the buffer contains at least the given number of bytes. If the buffer already has that much data or more, the caller is not suspended.
  * `co_await EvBuffer::write`: Writes the given number of bytes from the buffer. This does not drain them from the buffer.
* `HTTPServer`: Define a subclass of this, then implement handle_request. See HTTPServerExample.cc.
* `HTTPWebsocketServer`: As above, implement handle_request in a subclass. This class has websocket functions too; you can `co_await this->enable_websockets(req)` to convert the calling request into a websocket stream. enable_websockets returns a client object which you can use to send and receive websocket messages. (If enable_websockets returns nullptr then the request wasn't a websocket request, or it failed to convert it for some reason, and you should still call this->send_response as for a normal HTTP request.)
* `HTTPConnection`/`HTTPRequest`: These can be used to make outbound HTTP requests, optionally using OpenSSL. See HTTPClientExample.cc.

## Things to fix / improve / add

This library is a work in progress. I might make some breaking changes (e.g. to function or class definitions), but won't remove any functionality.

Some things to work on:
- Not all of the functions that can be `noexcept` are actually marked `noexcept`. There might be a couple of missing `const`s too.
- It seems like there should be a better way to handle HTTP responses than what we do currently. Unfortunately, improving this seems to require changing the evhttp_request's callback after creation time, which libevent doesn't allow.
- It would be nice to have awaitable subprocesses/pipes.
- It would be nice to have an easy and clean way to open SSL sockets and read/write from them (currently this is only supported for HTTP connections).
- The EvBuffer API feels suboptimal, but I can't put my finger on exactly how/why. Perhaps I'll figure this out after working with it some more.