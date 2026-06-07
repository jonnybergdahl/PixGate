#pragma once

// PixGate — widget registry. See DESIGN.md §7.2.
//
// Widget types self-register at static-init via PIXGATE_REGISTER_WIDGET, so the engine can
// instantiate by `type_id` from saved config and the web GUI can enumerate available types
// + schemas (via the JSON API) and stay in sync with the firmware automatically.

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "widget.h"

namespace esphome {
namespace pixgate {

using WidgetFactory = std::function<std::unique_ptr<Widget>()>;

// Process-wide registry of widget factories keyed by type_id. Implemented as a Meyers
// singleton so it is safe to populate from static initializers across translation units.
class WidgetRegistry {
 public:
  static WidgetRegistry &instance() {
    static WidgetRegistry inst;
    return inst;
  }

  // Register a factory for a type_id. Last registration wins (warn-worthy but harmless).
  void register_factory(const std::string &type_id, WidgetFactory factory) {
    this->factories_[type_id] = std::move(factory);
  }

  // Instantiate a widget by type_id, or nullptr if the type is unknown.
  std::unique_ptr<Widget> create(const std::string &type_id) const {
    auto it = this->factories_.find(type_id);
    if (it == this->factories_.end())
      return nullptr;
    return it->second();
  }

  bool has(const std::string &type_id) const { return this->factories_.count(type_id) > 0; }

  // All registered type ids (sorted by insertion into the map — i.e. alphabetical).
  std::vector<std::string> type_ids() const {
    std::vector<std::string> ids;
    ids.reserve(this->factories_.size());
    for (auto &kv : this->factories_)
      ids.push_back(kv.first);
    return ids;
  }

 private:
  WidgetRegistry() = default;
  std::map<std::string, WidgetFactory> factories_;
};

// Helper used by the registration macro: registers at construction (static-init) time.
struct WidgetRegistrar {
  WidgetRegistrar(const std::string &type_id, WidgetFactory factory) {
    WidgetRegistry::instance().register_factory(type_id, std::move(factory));
  }
};

// Register a Widget subclass under a string type_id. Place in the widget's .cpp:
//
//   PIXGATE_REGISTER_WIDGET("light", LightWidget);
//
#define PIXGATE_REGISTER_WIDGET(type_id_str, klass)                                  \
  static ::esphome::pixgate::WidgetRegistrar pixgate_registrar_##klass(              \
      type_id_str, []() -> std::unique_ptr<::esphome::pixgate::Widget> {             \
        return std::unique_ptr<::esphome::pixgate::Widget>(new klass());             \
      })

}  // namespace pixgate
}  // namespace esphome
