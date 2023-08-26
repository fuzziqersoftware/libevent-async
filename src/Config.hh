#pragma once

#include <event2/event.h>

#include <memory>

namespace EventAsync {

class Config {
public:
  Config();
  Config(const Config& config) = delete;
  Config(Config&& config) = default;
  Config& operator=(const Config& config) = delete;
  Config& operator=(Config&& config) = default;
  ~Config() = default;

  void avoid_method(const char* method);
  void require_features(enum event_method_feature features);
  void set_flag(enum event_base_config_flag flag);
  void set_num_cpus_hint(int cpus);
  void set_max_dispatch_interval(const struct timeval* max_interval,
      int max_callbacks, int min_priority);
  void set_max_dispatch_interval(uint64_t usecs, int max_callbacks,
      int min_priority);

  struct event_config* get();

protected:
  std::unique_ptr<struct event_config, void (*)(struct event_config*)> config;
};

} // namespace EventAsync
