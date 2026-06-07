#pragma once

// PixGate — runtime LVGL widget contract.
//
// This header defines the small, stable contract every widget implements, plus the
// supporting value types (EntityState, ConfigField/ConfigSchema). See DESIGN.md §7.
//
// The engine rides on ESPHome's `lvgl:` component, so widgets only ever touch the raw
// `lv_obj_*` C API on the active LVGL display — never the display/touch drivers directly.

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "lvgl.h"

#include "esphome/components/json/json_util.h"

namespace esphome {
namespace pixgate {

// A normalized snapshot of a Home Assistant entity's state, delivered to widgets.
struct EntityState {
  std::string entity_id;
  std::string state;                              // raw state string ("on", "23.4", ...)
  std::map<std::string, std::string> attributes;  // e.g. brightness, temperature, hvac_action
  bool available{false};

  // Convenience: fetch an attribute or a fallback if it is missing.
  std::string attr(const std::string &key, const std::string &fallback = "") const {
    auto it = this->attributes.find(key);
    return it == this->attributes.end() ? fallback : it->second;
  }
};

// Describes one configurable field of a widget type. Serialized to JSON and sent to the
// web GUI so it can render the correct form control. This is what makes "add a widget
// type = one file" true: the GUI needs no per-type knowledge.
struct ConfigField {
  enum Type { ENTITY, STRING, BOOL, INT, ENUM, COLOR, ICON };

  std::string key;                   // json key in the widget config
  std::string label;                 // human label in the GUI
  Type type{STRING};                 // form-control type
  std::vector<std::string> options;  // for ENUM
  std::string default_value;
  bool required{false};

  static const char *type_to_string(Type t) {
    switch (t) {
      case ENTITY:
        return "entity";
      case STRING:
        return "string";
      case BOOL:
        return "bool";
      case INT:
        return "int";
      case ENUM:
        return "enum";
      case COLOR:
        return "color";
      case ICON:
        return "icon";
    }
    return "string";
  }
};
using ConfigSchema = std::vector<ConfigField>;

// Forward declaration: widgets translate LVGL events into HA intents through this service.
class BindingService;

// The widget contract. Designed against `climate` (not `switch`) so complex widgets fit
// without an early refactor — see DESIGN.md §7.4.
class Widget {
 public:
  virtual ~Widget() = default;

  // Identity — must be globally unique and stable (stored in saved configs).
  virtual const char *type_id() const = 0;

  // What entity domain(s) this widget can bind to (used to filter the GUI entity picker).
  virtual std::vector<std::string> supported_domains() const = 0;

  // Construct the LVGL subtree under `parent` from this widget's slice of config.
  virtual void build(lv_obj_t *parent, JsonObjectConst cfg) = 0;

  // Apply a new HA state to the visuals. Called whenever the bound entity changes.
  virtual void on_state(const EntityState &s) = 0;

  // Handle an LVGL event (touch/slider/etc.) and translate it to an intent (service call
  // via the BindingService). Registered as the lv_event_cb during build().
  virtual void on_event(lv_event_t *e) = 0;

  // Self-description for the web GUI form.
  virtual const ConfigSchema &schema() const = 0;

  // Teardown: delete LVGL objects this widget created. Called on rebuild/remove.
  virtual void destroy() = 0;

  // --- Engine plumbing (not overridden by widgets) -------------------------------------

  void set_binding_service(BindingService *binding) { this->binding_ = binding; }
  BindingService *binding() const { return this->binding_; }

  // The entity_id this widget is bound to, if any (parsed from cfg during build()).
  const std::string &entity_id() const { return this->entity_id_; }
  void set_entity_id(const std::string &id) { this->entity_id_ = id; }

  // Stable per-instance id from the saved config (e.g. "w1").
  const std::string &instance_id() const { return this->instance_id_; }
  void set_instance_id(const std::string &id) { this->instance_id_ = id; }

 protected:
  BindingService *binding_{nullptr};
  std::string entity_id_;
  std::string instance_id_;
};

}  // namespace pixgate
}  // namespace esphome
