#pragma once

// PixGate — runtime Home Assistant binding (DESIGN.md §8).
//
// Because entities are chosen in the web GUI at runtime, we cannot use ESPHome's
// compile-time `homeassistant:` platforms (they need entity IDs at build time). This
// service wraps ESPHome's native API client layer to:
//   * subscribe to arbitrary entity states (state + a curated set of attributes),
//   * call arbitrary HA services with arbitrary entity_ids,
//   * cache the latest EntityState so a freshly built widget can be seeded immediately,
//   * dedupe multiple widgets bound to the same entity behind one underlying subscription.

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "widget.h"

namespace esphome {
namespace pixgate {

class BindingService {
 public:
  using StateCallback = std::function<void(const EntityState &)>;

  // Token returned from subscribe(); pass back to unsubscribe().
  using SubscriptionHandle = uint32_t;
  static constexpr SubscriptionHandle INVALID_HANDLE = 0;

  // Attributes that we always request for an entity. Widgets read whatever they need from
  // EntityState::attributes; requesting a small superset keeps the wire traffic bounded.
  void set_tracked_attributes(std::vector<std::string> attributes) {
    this->tracked_attributes_ = std::move(attributes);
  }

  // Subscribe a callback to an entity's state. The callback is invoked immediately with the
  // cached state if one is already known, and again on every subsequent change.
  SubscriptionHandle subscribe(const std::string &entity_id, StateCallback callback);

  // Remove a previously registered subscription.
  void unsubscribe(SubscriptionHandle handle);

  // Call a Home Assistant service, e.g. call_service("light", "toggle", {{"entity_id", ...}}).
  void call_service(const std::string &domain, const std::string &service,
                    const std::map<std::string, std::string> &data);

  // Latest known state for an entity (available=false if never seen).
  EntityState get_cached(const std::string &entity_id) const;

 protected:
  struct Subscriber {
    SubscriptionHandle handle;
    StateCallback callback;
  };

  struct EntitySub {
    EntityState state;
    std::vector<Subscriber> subscribers;
    bool wired{false};  // whether the underlying API subscription has been registered
  };

  // Wire up the underlying ESPHome API subscription for an entity (state + attributes).
  void wire_entity_(const std::string &entity_id, EntitySub &sub);

  // Push the current cached EntityState to all subscribers of an entity.
  void notify_(const std::string &entity_id);

  std::map<std::string, EntitySub> entities_;
  std::vector<std::string> tracked_attributes_{
      "brightness", "color_temp", "temperature", "current_temperature",
      "hvac_action", "hvac_modes", "unit_of_measurement", "friendly_name"};
  SubscriptionHandle next_handle_{1};
};

}  // namespace pixgate
}  // namespace esphome
