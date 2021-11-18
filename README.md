# libevent-async

libevent-async is a C++ wrapper around the libevent API. It wraps commonly-used objects in classes and provides coroutine support for making and accepting connections, reading and writing data, and waiting for timeouts.

This library was inspired by [rnburn's coevent](https://github.com/rnburn/coevent). This library takes a different approach in that it attempts to expose as much of libevent's functionality as possible using coroutines and other modern paradigms. (In a few places this gets kind of messy, unfortunately.)

There are also clients for various common protocols built as libraries alongside libevent-async. Currently, these protocols are:
* HTTP 1.1 (client supports SSL; server supports SSL and Websockets)
* MySQL (SQL queries + binlog streams)
* Memcache

## The libevent-async library

Everything here is in the namespace `EventAsync`.

* `Task<ReturnT>`: The common coroutine task type. Functions defined with this type are coroutines that co_return the specified type (which may be void). Execution does not begin until the task is co_awaited, and tasks are not destroyed automatically upon returning.
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
  * `co_await EvBuffer::read_atmost`: Reads up to the given number of bytes from the given fd and adds it to the buffer. This awaiter resumes when *any* nonzero amount of data is read, which may be less than the amount requested.
  * `co_await EvBuffer::read`: Reads the given number of bytes from the given fd and adds it to the buffer. This awaiter *does not* resume until the requested number of bytes have been read.
  * `co_await EvBuffer::read_to`: Reads enough bytes from the given fd such that the buffer contains at least the given number of bytes. If the buffer already has that much data or more, the caller is not suspended.
  * `co_await EvBuffer::write`: Writes the given number of bytes from the buffer to the given fd. If size is not given or is negative, writes the entire contents of the buffer. The written data is drained from the buffer.

## The libhttp-async library

This library exists in the namespace `EventAsync::HTTP`.

* `Server`: If you want to serve HTTP, HTTPS, or Websocket traffic, define a subclass of this and implement handle_request. See Protocols/HTTP/ServerExample.cc.
* `Connection`/`Request`: These can be used to make outbound HTTP requests, optionally using OpenSSL. See Protocols/HTTP/ClientExample.cc.

## The libmysql-async library

This library provides the class `EventAsync::MySQL::Client`. This client only supports caching_sha2_password authentication and can only do a few useful things; fortunately, one of those useful things is running SQL queries and returning result sets. See Protocols/MySQL/Client.hh for usage information.

## The libmemcache-async library

This library provides the class `EventAsync::Memcache::Client`. This client supports all the basic operations, but does not support virtual buckets or SASL authentication. See Protocols/Memcache/Client.hh for usage information.

## Things to fix / improve / add

This library is a work in progress. I might make some breaking changes (e.g. to function or class definitions), but won't remove any functionality.

Some things to work on:
- Not all of the functions that can be `noexcept` are actually marked `noexcept`. There might be a couple of missing `const`s too.
- It seems like there should be a better way to handle HTTP responses than what we do currently. Unfortunately, improving this seems to require changing the evhttp_request's callback after creation time, which libevent doesn't allow.
- It would be nice to have awaitable subprocesses/pipes.
- It would be nice to have an easy and clean way to open SSL sockets and read/write from them (currently this is only supported for HTTP connections).
- The EvBuffer API feels suboptimal, but I can't put my finger on exactly how/why. Perhaps I'll figure this out after working with it some more.