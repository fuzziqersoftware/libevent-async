#include "EventConfig.hh"

#include <phosg/Time.hh>

using namespace std;



EventConfig::EventConfig() : config(event_config_new(), event_config_free) {
  if (!this->config.get()) {
    throw runtime_error("event_config_new");
  }
}

void EventConfig::avoid_method(const char* method) {
  if (event_config_avoid_method(this->config.get(), method)) {
    throw runtime_error("event_config_avoid_method");
  }
}

void EventConfig::require_features(enum event_method_feature features) {
  if (event_config_require_features(this->config.get(), features)) {
    throw runtime_error("event_config_require_features");
  }
}

void EventConfig::set_flag(enum event_base_config_flag flag) {
  if (event_config_set_flag(this->config.get(), flag)) {
    throw runtime_error("event_config_set_flag");
  }
}

void EventConfig::set_num_cpus_hint(int cpus) {
  if (event_config_set_num_cpus_hint(this->config.get(), cpus)) {
    throw runtime_error("event_config_set_num_cpus_hint");
  }
}

void EventConfig::set_max_dispatch_interval(uint64_t max_interval,
    int max_callbacks, int min_priority) {
  auto max_interval_tv = usecs_to_timeval(max_interval);
  if (event_config_set_max_dispatch_interval(this->config.get(),
      &max_interval_tv, max_callbacks, min_priority)) {
    throw runtime_error("event_config_set_max_dispatch_interval");
  }
}

void EventConfig::set_max_dispatch_interval(const struct timeval* max_interval,
    int max_callbacks, int min_priority) {
  if (event_config_set_max_dispatch_interval(this->config.get(),
      max_interval, max_callbacks, min_priority)) {
    throw runtime_error("event_config_set_max_dispatch_interval");
  }
}

struct event_config* EventConfig::get() {
  return this->config.get();
}
