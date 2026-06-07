#pragma once

// PixGate — component entry, screen zones and lifecycle (DESIGN.md §5, §7.3).
//
// PixGate rides on ESPHome's `lvgl:` component: it never references the display or touch
// drivers, only the raw `lv_obj_*` C API on the active LVGL display. It builds a single root
// screen of three vertical zones (header / badge row / main window) and instantiates the
// runtime widget tree from the on-device JSON config.

#include <memory>
#include <string>
#include <vector>

#include "lvgl.h"

#include "esphome/core/component.h"

#include "binding.h"
#include "layout.h"
#include "registry.h"
#include "storage.h"
#include "widget.h"

namespace esphome {
namespace pixgate {

class PixGate : public Component {
 public:
  // --- ESPHome configuration setters (called from codegen in __init__.py) --------------
  void set_config_path(const std::string &path) { this->storage_.set_path(path); }

  // --- Component lifecycle --------------------------------------------------------------
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  // --- Public API surfaced to the web layer (DESIGN.md §11) -----------------------------

  // Current dashboard JSON document.
  std::string get_config_json() { return this->config_json_; }

  // Replace the dashboard JSON: validate, persist atomically, rebuild affected zones.
  // Returns false if the document is invalid (the live dashboard is left untouched).
  bool apply_config_json(const std::string &json);

  // Registry description (type_id + schema + supported_domains) as a JSON array.
  std::string get_registry_json();

  // Device geometry/version info as JSON (board width/height, version).
  std::string get_device_json();

  BindingService &binding() { return this->binding_; }

 protected:
  // Build the three root zones (header / badges / main) on the active LVGL screen.
  void build_root_();

  // Tear down every widget and clear the zone containers (teardown half of rebuild).
  void teardown_();

  // (Re)build the whole widget tree from `config_json_`.
  void rebuild_();

  // Build the widgets listed in a zone's "widgets" JSON array into `parent`.
  void build_zone_widgets_(lv_obj_t *parent, const void *widgets_array, bool is_grid,
                           GridLayout *grid);

  // Attach a freshly built widget to the binding service for its entity (seeds it too).
  void attach_widget_(Widget *w);

  ConfigStorage storage_;
  BindingService binding_;

  std::string config_json_;
  bool needs_rebuild_{false};

  // Active LVGL objects we own (children of the active screen).
  lv_obj_t *root_{nullptr};
  lv_obj_t *header_{nullptr};
  lv_obj_t *badges_{nullptr};
  lv_obj_t *main_{nullptr};

  GridLayout grid_;

  // Live widgets and their binding handles, so teardown can clean up deterministically.
  struct LiveWidget {
    std::unique_ptr<Widget> widget;
    BindingService::SubscriptionHandle handle{BindingService::INVALID_HANDLE};
  };
  std::vector<LiveWidget> widgets_;
};

}  // namespace pixgate
}  // namespace esphome
