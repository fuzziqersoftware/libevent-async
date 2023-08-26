#pragma once

#include <event2/event.h>
#include <event2/buffer.h>

#include <memory>
#include <string>
#include <coroutine>
#include <phosg/Encoding.hh>

#include "Base.hh"
#include "Event.hh"



namespace EventAsync {

struct Buffer {
  explicit Buffer(Base& base);
  Buffer(Base& base, struct evbuffer* buf);
  Buffer(const Buffer& buf) = delete;
  Buffer(Buffer&& buf);
  Buffer& operator=(const Buffer& buf) = delete;
  Buffer& operator=(Buffer&& buf) = delete;
  virtual ~Buffer();

  void enable_locking(void* lock);
  void lock();
  void unlock();

  size_t get_length() const;
  size_t get_contiguous_space() const;
  void expand(size_t size);

  void add(const void* data, size_t size);
  void add(const std::string& data);

  size_t add_printf(const char* fmt, ...)
  __attribute__((format(printf, 2, 3)));
  size_t add_vprintf(const char* fmt, va_list va);

  void add_buffer(struct evbuffer* src);
  void add_buffer(Buffer& src);
  size_t remove_buffer(struct evbuffer* src, size_t size);
  size_t remove_buffer(Buffer& src, size_t size);

  void prepend(const void* data, size_t size);
  void prepend_buffer(struct evbuffer* src);
  void prepend_buffer(Buffer& src);

  uint8_t* pullup(ssize_t size);

  void drain(size_t size);
  void drain_all();

  size_t remove_atmost(void* data, size_t size);
  std::string remove_atmost(size_t size);
  void remove(void* data, size_t size);
  std::string remove(size_t size);

  size_t copyout_atmost(void* data, size_t size);
  std::string copyout_atmost(size_t size);
  void copyout(void* data, size_t size);
  std::string copyout(size_t size);
  size_t copyout_from_atmost(const struct evbuffer_ptr* pos, void* data, size_t size);
  std::string copyout_from_atmost(const struct evbuffer_ptr* pos, size_t size);
  void copyout_from(const struct evbuffer_ptr* pos, void* data, size_t size);
  std::string copyout_from(const struct evbuffer_ptr* pos, size_t size);

  template <typename T> void add(const T& t) {
    this->add(&t, sizeof(T));
  }
  template <typename T> T remove() {
    T ret;
    this->remove(&ret, sizeof(T));
    return ret;
  }
  template <typename T> T copyout() {
    T ret;
    this->copyout(&ret, sizeof(T));
    return ret;
  }

  inline void add_u8(uint8_t v)    { this->add<uint8_t>(v); }
  inline void add_s8(int8_t v)     { this->add<int8_t>(v); }
  inline void add_u16b(uint16_t v) { this->add<be_uint16_t>(v); }
  inline void add_s16b(int16_t v)  { this->add<be_int16_t>(v); }
  inline void add_u16l(uint16_t v) { this->add<le_uint16_t>(v); }
  inline void add_s16l(int16_t v)  { this->add<le_int16_t>(v); }
  inline void add_u32b(uint32_t v) { this->add<be_uint32_t>(v); }
  inline void add_s32b(int32_t v)  { this->add<be_int32_t>(v); }
  inline void add_u32l(uint32_t v) { this->add<le_uint32_t>(v); }
  inline void add_s32l(int32_t v)  { this->add<le_int32_t>(v); }
  inline void add_u64b(uint64_t v) { this->add<be_uint64_t>(v); }
  inline void add_s64b(int64_t v)  { this->add<be_int64_t>(v); }
  inline void add_u64l(uint64_t v) { this->add<le_uint64_t>(v); }
  inline void add_s64l(int64_t v)  { this->add<le_int64_t>(v); }

  inline uint8_t remove_u8()    { return this->remove<uint8_t>(); }
  inline int8_t remove_s8()     { return this->remove<int8_t>(); }
  inline uint16_t remove_u16b() { return this->remove<be_uint16_t>(); }
  inline int16_t remove_s16b()  { return this->remove<be_int16_t>(); }
  inline uint16_t remove_u16l() { return this->remove<le_uint16_t>(); }
  inline int16_t remove_s16l()  { return this->remove<le_int16_t>(); }
  inline uint32_t remove_u32b() { return this->remove<be_uint32_t>(); }
  inline int32_t remove_s32b()  { return this->remove<be_int32_t>(); }
  inline uint32_t remove_u32l() { return this->remove<le_uint32_t>(); }
  inline int32_t remove_s32l()  { return this->remove<le_int32_t>(); }
  inline uint64_t remove_u64b() { return this->remove<be_uint64_t>(); }
  inline int64_t remove_s64b()  { return this->remove<be_int64_t>(); }
  inline uint64_t remove_u64l() { return this->remove<le_uint64_t>(); }
  inline int64_t remove_s64l()  { return this->remove<le_int64_t>(); }

  inline uint8_t copyout_u8()    { return this->copyout<uint8_t>(); }
  inline int8_t copyout_s8()     { return this->copyout<int8_t>(); }
  inline uint16_t copyout_u16b() { return this->copyout<be_uint16_t>(); }
  inline int16_t copyout_s16b()  { return this->copyout<be_int16_t>(); }
  inline uint16_t copyout_u16l() { return this->copyout<le_uint16_t>(); }
  inline int16_t copyout_s16l()  { return this->copyout<le_int16_t>(); }
  inline uint32_t copyout_u32b() { return this->copyout<be_uint32_t>(); }
  inline int32_t copyout_s32b()  { return this->copyout<be_int32_t>(); }
  inline uint32_t copyout_u32l() { return this->copyout<le_uint32_t>(); }
  inline int32_t copyout_s32l()  { return this->copyout<le_int32_t>(); }
  inline uint64_t copyout_u64b() { return this->copyout<be_uint64_t>(); }
  inline int64_t copyout_s64b()  { return this->copyout<be_int64_t>(); }
  inline uint64_t copyout_u64l() { return this->copyout<le_uint64_t>(); }
  inline int64_t copyout_s64l()  { return this->copyout<le_int64_t>(); }

  std::string readln(enum evbuffer_eol_style eol_style);

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

  void add_reference(const std::string& data);
  void add_reference(const void* data, size_t size,
      void (*cleanup_fn)(const void* data, size_t size, void* ctx) = nullptr,
      void* ctx = nullptr);
  void add_reference(const void* data, size_t size,
      std::function<void(const void* data, size_t size)> cleanup_fn);
  void add(std::string&& data);

  void add_file(int fd, off_t offset, size_t size);

  void add_buffer_reference(struct evbuffer* other_buf);
  void add_buffer_reference(Buffer& other_buf);

  void freeze(int at_front);
  void unfreeze(int at_front);

  // TODO: reduce code duplication between these awaiter classes

  class ReadAwaiter {
  public:
    ReadAwaiter(const ReadAwaiter&) = delete;
    ReadAwaiter(ReadAwaiter&&) = delete;
    ReadAwaiter& operator=(const ReadAwaiter&) = delete;
    ReadAwaiter& operator=(ReadAwaiter&&) = delete;
    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> coro);
  protected:
    ReadAwaiter(
        Buffer& buf,
        evutil_socket_t fd,
        ssize_t limit,
        void (*cb)(evutil_socket_t, short, void*));
    Buffer& buf;
    Event event;
    size_t limit;
    size_t bytes_read;
    bool err;
    std::coroutine_handle<> coro;
  };

  class ReadAtMostAwaiter : public ReadAwaiter {
  public:
    ReadAtMostAwaiter(
        Buffer& buf,
        evutil_socket_t fd,
        ssize_t limit);
    size_t await_resume();
  private:
    static void on_read_ready(evutil_socket_t fd, short what, void* ctx);
  };

  class ReadExactlyAwaiter : public ReadAwaiter {
  public:
    ReadExactlyAwaiter(
        Buffer& buf,
        evutil_socket_t fd,
        size_t limit);
    void await_resume();
  private:
    bool eof;
    static void on_read_ready(evutil_socket_t fd, short what, void* ctx);
  };

  ReadAtMostAwaiter read_atmost(evutil_socket_t fd, ssize_t size = -1);
  ReadExactlyAwaiter read(evutil_socket_t fd, size_t size);
  ReadExactlyAwaiter read_to(evutil_socket_t fd, size_t size);

  class WriteAwaiter {
  public:
    WriteAwaiter(
        Buffer& buf,
        evutil_socket_t fd,
        ssize_t size);
    WriteAwaiter(const WriteAwaiter&) = delete;
    WriteAwaiter(WriteAwaiter&&) = delete;
    WriteAwaiter& operator=(const WriteAwaiter&) = delete;
    WriteAwaiter& operator=(WriteAwaiter&&) = delete;
    bool await_ready() const noexcept;
    void await_suspend(std::coroutine_handle<> coro);
    void await_resume();
  protected:
    Buffer& buf;
    Event event;
    ssize_t limit;
    size_t bytes_written;
    bool err;
    std::coroutine_handle<> coro;
    static void on_write_ready(evutil_socket_t fd, short what, void* ctx);
  };

  WriteAwaiter write(evutil_socket_t fd, ssize_t size = -1);

  // This copies all of the data out, so it is slow and should only be used for
  // debugging
  void debug_print_contents(FILE* stream);

  Base& base;
  struct evbuffer* buf;
  bool owned;
};

} // namespace EventAsync
