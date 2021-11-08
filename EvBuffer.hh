#pragma once

#include <event2/event.h>
#include <event2/buffer.h>

#include <memory>
#include <string>
#include <experimental/coroutine>

#include "EventBase.hh"
#include "Event.hh"



struct EvBuffer {
  explicit EvBuffer(EventBase& base);
  EvBuffer(EventBase& base, struct evbuffer* buf);
  EvBuffer(const EvBuffer& buf) = delete;
  EvBuffer(EvBuffer&& buf);
  EvBuffer& operator=(const EvBuffer& buf) = delete;
  EvBuffer& operator=(EvBuffer&& buf) = delete;
  virtual ~EvBuffer();

  void enable_locking(void* lock);
  void lock();
  void unlock();

  size_t get_length() const;
  size_t get_contiguous_space() const;
  void expand(size_t size);

  void add(const void* data, size_t size);

  size_t add_printf(const char* fmt, ...);
  size_t add_vprintf(const char* fmt, va_list va);

  void add_buffer(struct evbuffer* src);
  void add_buffer(EvBuffer& src);
  size_t remove_buffer(struct evbuffer* src, size_t size);
  size_t remove_buffer(EvBuffer& src, size_t size);

  void prepend(const void* data, size_t size);
  void prepend_buffer(struct evbuffer* src);
  void prepend_buffer(EvBuffer& src);

  uint8_t* pullup(ssize_t size);

  void drain(size_t size);
  void drain_all();

  size_t remove(void* data, size_t size);
  std::string remove(size_t size);
  void remove_exactly(void* data, size_t size);
  std::string remove_exactly(size_t size);

  size_t copyout(void* data, size_t size);
  std::string copyout(size_t size);
  size_t copyout_from(const struct evbuffer_ptr* pos, void* data, size_t size);
  std::string copyout_from(const struct evbuffer_ptr* pos, size_t size);

  std::unique_ptr<char, void(*)(void*)> readln(size_t* bytes_read,
      enum evbuffer_eol_style eol_style);

  struct evbuffer_ptr search(const char* what, size_t size,
      const struct evbuffer_ptr* start);
  struct evbuffer_ptr search_range(const char* what, size_t size,
      const struct evbuffer_ptr *start, const struct evbuffer_ptr *end);
  struct evbuffer_ptr search_eol(struct evbuffer_ptr* start,
      size_t* bytes_found, enum evbuffer_eol_style eol_style);
  void ptr_set(struct evbuffer_ptr* pos, size_t position,
      enum evbuffer_ptr_how how);

  int peek(ssize_t size, struct evbuffer_ptr* start_at,
      struct evbuffer_iovec* vec_out, int n_vec);
  int reserve_space(ev_ssize_t size, struct evbuffer_iovec* vec, int n_vecs);
  void commit_space(struct evbuffer_iovec* vec, int n_vecs);

  void add_reference(const void* data, size_t size,
      void (*cleanup_fn)(const void* data, size_t size, void* ctx) = nullptr,
      void* ctx = nullptr);
  void add_reference(const void* data, size_t size,
      std::function<void(const void* data, size_t size)> cleanup_fn);
  void add(std::string&& data);

  void add_file(int fd, off_t offset, size_t size);

  void add_buffer_reference(struct evbuffer* other_buf);
  void add_buffer_reference(EvBuffer& other_buf);

  void freeze(int at_front);
  void unfreeze(int at_front);

  // TODO: reduce code duplication between these awaiter classes

  class ReadAwaiter {
  public:
    bool await_ready() const noexcept;
    void await_suspend(std::experimental::coroutine_handle<> coro);
  protected:
    ReadAwaiter(
        EvBuffer& buf,
        evutil_socket_t fd,
        ssize_t limit,
        void (*cb)(evutil_socket_t, short, void*));
    EvBuffer& buf;
    Event event;
    size_t limit;
    size_t bytes_read;
    bool err;
    std::experimental::coroutine_handle<> coro;
  };

  class ReadAtMostAwaiter : public ReadAwaiter {
  public:
    ReadAtMostAwaiter(
        EvBuffer& buf,
        evutil_socket_t fd,
        ssize_t limit);
    size_t await_resume();
  private:
    static void on_read_ready(evutil_socket_t fd, short what, void* ctx);
  };

  class ReadExactlyAwaiter : public ReadAwaiter {
  public:
    ReadExactlyAwaiter(
        EvBuffer& buf,
        evutil_socket_t fd,
        size_t limit);
    void await_resume();
  private:
    bool eof;
    static void on_read_ready(evutil_socket_t fd, short what, void* ctx);
  };

  ReadAtMostAwaiter read(evutil_socket_t fd, ssize_t size = -1);
  ReadExactlyAwaiter read_exactly(evutil_socket_t fd, size_t size);
  ReadExactlyAwaiter read_to(evutil_socket_t fd, size_t size);

  class WriteAwaiter {
  public:
    WriteAwaiter(
        EvBuffer& buf,
        evutil_socket_t fd,
        size_t size);
    bool await_ready() const noexcept;
    void await_suspend(std::experimental::coroutine_handle<> coro);
    void await_resume();
  protected:
    EvBuffer& buf;
    Event event;
    ssize_t limit;
    size_t bytes_written;
    bool err;
    std::experimental::coroutine_handle<> coro;
    static void on_write_ready(evutil_socket_t fd, short what, void* ctx);
  };

  WriteAwaiter write(evutil_socket_t fd, ssize_t size = -1);

  // This copies all of the data out, so it is slow and should only be used for
  // debugging
  void debug_print_contents(FILE* stream);

  EventBase& base;
  struct evbuffer* buf;
  bool owned;
};
