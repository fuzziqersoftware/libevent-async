#pragma once

#include <event2/event.h>

#include <memory>



class EventConfig {
public:
  EventConfig();
  EventConfig(const EventConfig& config) = delete;
  EventConfig(EventConfig&& config) = default;
  EventConfig& operator=(const EventConfig& config) = delete;
  EventConfig& operator=(EventConfig&& config) = default;
  ~EventConfig() = default;

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
  std::unique_ptr<struct event_config, void(*)(struct event_config*)> config;
};
