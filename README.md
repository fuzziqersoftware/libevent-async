# libevent-async

libevent-async is a C++ wrapper around the libevent API. It wraps commonly-used objects in classes and provides coroutine support for making and accepting connections, reading and writing data, and waiting for timeouts.

This library was inspired by [rnburn's coevent](https://github.com/rnburn/coevent). This library takes a different approach in that it attempts to expose as much of libevent's functionality as possible using coroutines and other modern paradigms. (In a few places this gets kind of messy, unfortunately.)

There are also clients for various common protocols built as libraries alongside libevent-async. Currently, these protocols are:
* HTTP 1.1 (client supports SSL; server supports SSL and Websockets)
* MySQL (SQL queries + binlog streams)
* Memcache

## The libevent-async library

Everything here is in the namespace `EventAsync`.

* Task types and functions (include `<event-async/Task.hh>`)
  * `Task<ReturnT>`: The common coroutine task type. Functions defined with this return type are coroutines that co_return the specified type (which may be void). Execution does not begin until the task is co_awaited or .start() is called.
  * `DetachedTask`: Used for tasks that execute independently of their callers. Before calling Base::run, call one or more DetachedTasks in order to create servers and whatnot. Unlike AsyncTasks, DetachedTasks begin executing immediately when they are called, and are automatically destroyed when their coroutine returns. They may not return a value.
  * `co_await multi(TaskT& t1, ...)`: Like all(), but takes references to task objects directly instead. Unlike all(), the passed-in tasks do not need to have the same return type.
  * `co_await all(Iterator start, Iterator end)`: Runs all of the tasks in parallel (assuming they all block on I/O at some point), and returns when all tasks have either returned or thrown an exception. all() does not return a value; the caller must either co_await each task to get the value or exception, or call .result() on each task, after all() returns.
  * `co_await all_limit(Iterator start, Iterator end, size_t parallelism)`: Similar to all(), but only runs up to a specific number of tasks at a time.
  * `completed_task = co_await any(Iterator start, Iterator end)`: Similar to all(), but returns when any of the given tasks has returned or thrown an exception. Returns a pointer to the first task that has completed. The remaining incomplete tasks are not canceled and will continue to run even if no one co_awaits them. If any task is already done when any() is called, it returns immediately; if multiple tasks are already done at call time, it is not defined which of them any() returns a pointer to.
* `Future<ResultT>` and `DeferredFuture<ResultT>` (include `<event-async/Future.hh>`)
  * Can be directly co_awaited, like a Task. Unlike a Task, multiple coroutines can co_await the same Future at the same time. The co_await expression waits for the future to be resolved, and returns the value it was resolved with, or throws the exception it was resolved with. ResultT may be void if blocking is needed but returning a value isn't necessary.
  * `future.result()`: Returns the future's value or throws its exception.
  * `future.exception()`: Returns (does not throw) the future's exception, or returns nullptr if the future is pending or has a value instead.
  * `future.done()`, `future.has_result()`, `future.has_exception()`: Return the state of the future.
  * `future.set_result(value)`: Sets the result value of the future and resumes all awaiting coroutines. Coroutines are resumed in the order they began awaiting.
  * `future.set_exception(exc)`: Sets the exception of the future and resumes all awaiting coroutines. The exception is thrown into all awaiting coroutines at the co_await expression.
  * `future.cancel()`: Cancels the future, throwing Future::canceled_error into all awaiting coroutines.
  * DeferredFuture is like Future, except the awaiting coroutines are resumed via callbacks on a Base's event loop, instead of being resumed immediately during the set_value/set_exception call. DeferredFutures are therefore attached to a Base (see below), whereas Futures are not.
* `Channel<ItemT>` (include `<event-async/Channel.hh>`)
  * A channel is an awaitable queue of objects, which can be used to pass messages between coroutines. Make a Channel object, then pass a pointer/reference to it into multiple coroutines.
  * `co_await channel.read()`: Dequeues an item from the queue. If the queue is empty, waits for someone to call .write() on it, then returns what they passed to .write().
  * `channel.write(item)`: Enqueues an item into the queue. If ItemT is nontrivial, you may instead want to use `channel.write(std::move(item))`. If another coroutine is waiting on the queue (because it is empty), wakes up that coroutine. If multiple coroutines are waiting, wakes the one that has been waiting the longest. If no coroutines are waiting on the queue, .write() does not block; the message will just sit in the queue until someone calls .read().
* `Base` (include `<event-async/Base.hh>`)
  * If you want to change options about the polling backend (for example), create a Config object first (`<event-async/Config.hh>`) and use that when constructing your Base. Config objects mirror the event_config functionality in libevent.
  * `base.run()`: Runs the event loop, just like event_base_dispatch().
  * `co_await base.sleep(microseconds)`: Suspends the caller for the given time.
  * `co_await base.read(fd, buffer, size)`: Reads data from a (nonblocking) file descriptor. If called as `base.read(fd, size)`, the data is returned in a std::string instead.
  * `co_await base.write(fd, data, size)`: Writes data to a (nonblocking) file descriptor. There is also `base.write(fd, data)` if data is a std::string.
  * `co_await base.connect(addr, port)`: Connects to a remote server. If you pass a hostname rather than an IP address, this will do a blocking DNS lookup. To avoid this, you can resolve the hostname using a DNSBase first.
  * `co_await base.accept(fd[, peer_addr])`: Waits for and returns an incoming connection.
* `Buffer` (include `<event-async/Buffer.hh>`)
  * All standard `evbuffer_*` functions are present as methods on this class as well.
  * `co_await buffer.read_atmost(fd[, size])`: Reads up to the given number of bytes from the given fd and adds it to the buffer. This awaiter resumes when *any* nonzero amount of data is read, which may be less than the amount requested.
  * `co_await buffer.read(fd, size)`: Reads the given number of bytes from the given fd and adds it to the buffer. This awaiter *does not* resume until the requested number of bytes have been read.
  * `co_await buffer.read_to(fd, size)`: Reads enough bytes from the given fd such that the buffer contains at least the given number of bytes. If the buffer already has that much data or more, this function does nothing.
  * `co_await buffer.write(fd[, size])`: Writes the given number of bytes from the buffer to the given fd. If size is not given or is negative, writes the entire contents of the buffer. The written data is drained from the buffer.
* `DNSBase` (include `<event-async/DNSBase.hh>`)
  * Most evdns_base functions are implemented as methods on this class. The DNSBase uses reasonable defaults at construction time, so it's not required to call any of the configuration functions.
  * `dns_base.getaddrinfo` is unfortunately not an awaiter yet. This will be fixed in the future.
  * `co_await dns_base.resolve_ipv4(name[, flags])`: Performs a forward IPv4 resolution. The returned object provides the A record addresses (struct in_addr) in .results. If .result_code in the returned object is nonzero, an error occurred and the results should probably be ignored.
  * `co_await dns_base.resolve_ipv6(name[, flags])`: Like resolve_ipv4, but returns AAAA records (struct in6_addr).
  * `co_await dns_base.resolve_reverse_ipv4(addr[, flags])`: Like resolve_ipv4, but takes an in_addr and returns PTR records (strings).
  * `co_await dns_base.resolve_reverse_ipv6(addr[, flags])`: Like resolve_ipv4, but takes an in6_addr and returns PTR records (strings).

## The libhttp-async library

This library exists in the namespace `EventAsync::HTTP`.

* `Server`: If you want to serve HTTP, HTTPS, or Websocket traffic, define a subclass of this and implement handle_request. Then instantiate your subclass and call add_socket one or more times before calling base.run(). See Examples/HTTPServer.cc and Examples/HTTPWebsocketServer.cc.
* `Connection`/`Request`: These can be used to make outbound HTTP requests, optionally using OpenSSL. See Examples/HTTPClient.cc.

To use these, include `<event-async/Protocols/HTTP/Server.hh>`, `<event-async/Protocols/HTTP/Connection.hh>`, and/or `<event-async/Protocols/HTTP/Request.hh>` and link with -lhttp-async.

## The libmysql-async library

This library provides the classes `EventAsync::MySQL::Client` and `EventAsync::MySQL::BinlogProcessor`. To use the client, make a Client object and `co_await client.connect()`; after that, you can `co_await client.query(...)` to run SQL. See Protocols/MySQL/Client.hh for usage information. The client currently only supports caching_sha2_password authentication.

You can also `co_await read_binlogs(...)` and `co_await get_binlog_event()` to read binlogs; to turn the binlog events into a more useful format, run them through a `BinlogProcessor` instance. See Examples/MySQLBinlogReader.cc and Examples/MySQLBinlogStats.cc for examples of this.

To use this, include `<event-async/Protocols/MySQL/Client.hh>` and link with -lmysql-async.

## The libmemcache-async library

This library provides the class `EventAsync::Memcache::Client`. This client supports all the basic operations, but does not support virtual buckets or SASL authentication. See Protocols/Memcache/Client.hh for usage information.

To use this, include `<event-async/Protocols/Memcache/Client.hh>` and link with -lmemcache-async.

## Things to fix / improve / add

This library is a work in progress. I might make some breaking changes (e.g. to function or class definitions), but won't remove any functionality.

Some things to work on:
- Not all of the functions that can be `noexcept` are actually marked `noexcept`. There might be a couple of missing `const`s too.
- It seems like there should be a better way to handle HTTP responses than what we do currently. Unfortunately, improving this seems to require changing the evhttp_request's callback after creation time, which libevent doesn't allow.
- It would be nice to have awaitable subprocesses.
- It would be nice to have an easy and clean way to open SSL sockets and read/write from them directly. Currently this is only supported for HTTP connections, since it's implemented in the C layer without coroutines.
