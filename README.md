# libevent-async

libevent-async is a C++ wrapper around the libevent API. It wraps commonly-used objects in classes and provides coroutine support for making and accepting connections, reading and writing data, and waiting for timeouts.

This library was inspired by [rnburn's coevent](https://github.com/rnburn/coevent). This library takes a different approach in that it attempts to expose as much of libevent's functionality as possible using coroutines and other modern paradigms. (In a few places this gets kind of messy, unfortunately.)

There are also clients for various common protocols built as libraries alongside libevent-async. Currently, these protocols are:
* HTTP 1.1 (client supports SSL; server supports SSL and Websockets)
* MySQL (SQL queries + binlog streams)
* Memcache

## The libevent-async library

Everything here is in the namespace `EventAsync`.

* Task types and functions
  * `Task<ReturnT>`: The common coroutine task type. Functions defined with this return type are coroutines that co_return the specified type (which may be void). Execution does not begin until the task is co_awaited or .start() is called.
  * `DetachedTask`: Used for tasks that execute independently of their callers. Before calling Base::run, call one or more DetachedTasks in order to create servers and whatnot. Unlike AsyncTasks, DetachedTasks begin executing immediately when they are called, and are automatically destroyed when their coroutine returns. They may not return a value.
  * `co_await all(Iterator start, Iterator end)`: Runs all of the tasks in parallel (assuming they all block on I/O at some point), and returns when all tasks have either returned or thrown an exception. all() does not return a value; the caller must either co_await each task or call .result() on each task after all() returns.
  * `co_await all_limit(Iterator start, Iterator end, size_t parallelism)`: Similar to all(), but only runs up to a specific number of tasks at a time.
  * `completed_task = co_await any(Iterator start, Iterator end)`: Similar to all(), but returns when any of the given tasks has returned or thrown an exception. Returns a pointer to the first task that has completed. The remaining incomplete tasks are not canceled and will continue to run even if no one co_awaits them. If any task is already done when any() is called, it returns immediately; if multiple tasks are already done at call time, it is not defined which of them any() returns a pointer to.
* `Base`
  * `run`: Runs the event loop, just like event_base_dispatch().
  * `co_await base.sleep`: Suspends the caller for the given time.
  * `co_await base.read`: Reads data from a (nonblocking) file descriptor.
  * `co_await base.write`: Writes data to a (nonblocking) file descriptor.
  * `co_await base.connect`: Connects to a remote server.
  * `co_await base.accept`: Waits for and returns an incoming connection.
* `Buffer`
  * All standard `evbuffer_*` functions are present as methods on this class as well.
  * `co_await buffer.read_atmost`: Reads up to the given number of bytes from the given fd and adds it to the buffer. This awaiter resumes when *any* nonzero amount of data is read, which may be less than the amount requested.
  * `co_await buffer.read`: Reads the given number of bytes from the given fd and adds it to the buffer. This awaiter *does not* resume until the requested number of bytes have been read.
  * `co_await buffer.read_to`: Reads enough bytes from the given fd such that the buffer contains at least the given number of bytes. If the buffer already has that much data or more, this function does nothing.
  * `co_await buffer.write`: Writes the given number of bytes from the buffer to the given fd. If size is not given or is negative, writes the entire contents of the buffer. The written data is drained from the buffer.

## The libhttp-async library

This library exists in the namespace `EventAsync::HTTP`.

* `Server`: If you want to serve HTTP, HTTPS, or Websocket traffic, define a subclass of this and implement handle_request. Then instantiate your subclass and call add_socket one or more times before calling base.run(). See Protocols/HTTP/ServerExample.cc.
* `Connection`/`Request`: These can be used to make outbound HTTP requests, optionally using OpenSSL. See Protocols/HTTP/ClientExample.cc.

To use these, include `<event-async/Protocols/HTTP/Server.hh>`, `<event-async/Protocols/HTTP/Connection.hh>`, and/or `<event-async/Protocols/HTTP/Request.hh>` and link with -lhttp-async.

## The libmysql-async library

This library provides the classes `EventAsync::MySQL::Client` and `EventAsync::MySQL::BinlogProcessor`. To use the client, make a Client object and `co_await client.connect()`; after that, you can `co_await client.query(...)` to run SQL. See Protocols/MySQL/Client.hh for usage information. The client currently only supports caching_sha2_password authentication.

You can also `co_await read_binlogs(...)` and `co_await get_binlog_event()` to read binlogs; to turn the binlog events into a more useful format, run them through a `BinlogProcessor` instance. See Protocols/MySQL/BinlogReader.cc for an example of this.

To use this, include `<event-async/Protocols/MySQL/Client.hh>` and link with -lmysql-async.

## The libmemcache-async library

This library provides the class `EventAsync::Memcache::Client`. This client supports all the basic operations, but does not support virtual buckets or SASL authentication. See Protocols/Memcache/Client.hh for usage information.

To use this, include `<event-async/Protocols/Memcache/Client.hh>` and link with -lmemcache-async.

## Things to fix / improve / add

This library is a work in progress. I might make some breaking changes (e.g. to function or class definitions), but won't remove any functionality.

Some things to work on:
- Not all of the functions that can be `noexcept` are actually marked `noexcept`. There might be a couple of missing `const`s too.
- It seems like there should be a better way to handle HTTP responses than what we do currently. Unfortunately, improving this seems to require changing the evhttp_request's callback after creation time, which libevent doesn't allow.
- It would be nice to have awaitable subprocesses/pipes.
- It would be nice to have an easy and clean way to open SSL sockets and read/write from them directly (currently this is only supported for HTTP connections, since it's implemented there in the C layer without coroutines).
