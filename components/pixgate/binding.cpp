#include "binding.h"

#include "esphome/core/log.h"

#ifdef USE_API
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/api_pb2.h"
#endif

namespace esphome {
namespace pixgate {

static const char *const TAG = "pixgate.binding";

BindingService::SubscriptionHandle BindingService::subscribe(const std::string &entity_id,
                                                             StateCallback callback) {
  if (entity_id.empty())
    return INVALID_HANDLE;

  EntitySub &sub = this->entities_[entity_id];
  SubscriptionHandle handle = this->next_handle_++;
  sub.subscribers.push_back(Subscriber{handle, std::move(callback)});

  // Lazily register the underlying API subscription the first time someone cares.
  if (!sub.wired)
    this->wire_entity_(entity_id, sub);

  // Seed the new subscriber immediately if we already know a state.
  if (sub.state.available) {
    sub.subscribers.back().callback(sub.state);
  }
  return handle;
}

void BindingService::unsubscribe(SubscriptionHandle handle) {
  if (handle == INVALID_HANDLE)
    return;
  for (auto &kv : this->entities_) {
    auto &subs = kv.second.subscribers;
    for (auto it = subs.begin(); it != subs.end(); ++it) {
      if (it->handle == handle) {
        subs.erase(it);
        return;
      }
    }
  }
}

EntityState BindingService::get_cached(const std::string &entity_id) const {
  auto it = this->entities_.find(entity_id);
  if (it == this->entities_.end())
    return EntityState{entity_id, "", {}, false};
  return it->second.state;
}

void BindingService::notify_(const std::string &entity_id) {
  auto it = this->entities_.find(entity_id);
  if (it == this->entities_.end())
    return;
  // Copy the subscriber list defensively: a callback could rebuild the tree and mutate it.
  auto subscribers = it->second.subscribers;
  const EntityState state = it->second.state;
  for (auto &s : subscribers)
    s.callback(state);
}

void BindingService::wire_entity_(const std::string &entity_id, EntitySub &sub) {
  sub.wired = true;
  sub.state.entity_id = entity_id;

#ifdef USE_API
  if (api::global_api_server == nullptr) {
    ESP_LOGW(TAG, "API server not available; '%s' will not receive state updates",
             entity_id.c_str());
    return;
  }

  // Subscribe to the bare state (no attribute argument). The callback type is spelled out
  // explicitly to disambiguate the StringRef vs. std::string subscribe overloads.
  api::global_api_server->subscribe_home_assistant_state(
      entity_id, optional<std::string>(),
      std::function<void(const std::string &)>([this, entity_id](const std::string &state) {
        auto it = this->entities_.find(entity_id);
        if (it == this->entities_.end())
          return;
        it->second.state.state = state;
        it->second.state.available = true;
        this->notify_(entity_id);
      }));

  // Subscribe to each tracked attribute separately.
  for (const std::string &attr : this->tracked_attributes_) {
    api::global_api_server->subscribe_home_assistant_state(
        entity_id, optional<std::string>(attr),
        std::function<void(const std::string &)>([this, entity_id, attr](const std::string &value) {
          auto it = this->entities_.find(entity_id);
          if (it == this->entities_.end())
            return;
          if (value.empty()) {
            it->second.state.attributes.erase(attr);
          } else {
            it->second.state.attributes[attr] = value;
          }
          this->notify_(entity_id);
        }));
  }
#else
  ESP_LOGW(TAG, "Compiled without USE_API; entity binding disabled");
#endif
}

void BindingService::call_service(const std::string &domain, const std::string &service,
                                  const std::map<std::string, std::string> &data) {
  const std::string service_name = domain + "." + service;
  ESP_LOGD(TAG, "call_service %s", service_name.c_str());

#ifdef USE_API
  if (api::global_api_server == nullptr) {
    ESP_LOGW(TAG, "API server not available; dropping service call %s", service_name.c_str());
    return;
  }

  // HomeassistantActionRequest holds StringRefs into externally-owned storage; service_name and
  // the caller's `data` map both outlive this synchronous send, so referencing them is safe.
  api::HomeassistantActionRequest resp;
  resp.service = StringRef(service_name);
  resp.is_event = false;
  resp.data.init(data.size());
  for (const auto &kv : data) {
    auto &entry = resp.data.emplace_back();
    entry.key = StringRef(kv.first);
    entry.value = StringRef(kv.second);
  }
  api::global_api_server->send_homeassistant_action(resp);
#else
  ESP_LOGW(TAG, "Compiled without USE_API; dropping service call %s", service_name.c_str());
#endif
}

}  // namespace pixgate
}  // namespace esphome
