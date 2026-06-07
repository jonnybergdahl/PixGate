// PixGate — sensor widget (DESIGN.md §10).
//
// Read-only value tile: shows the state plus its unit_of_measurement. Works for both
// `sensor` and `binary_sensor` domains. No service calls.

#include "widget_base.h"
#include "registry.h"

namespace esphome {
namespace pixgate {

class SensorWidget : public TileWidget {
 public:
  const char *type_id() const override { return "sensor"; }

  std::vector<std::string> supported_domains() const override {
    return {"sensor", "binary_sensor"};
  }

  const ConfigSchema &schema() const override {
    static const ConfigSchema s = {
        {"entity_id", "Entity", ConfigField::ENTITY, {}, "", true},
        {"label", "Label", ConfigField::STRING, {}, "", false},
        {"icon", "Icon", ConfigField::ICON, {}, "mdi:gauge", false},
    };
    return s;
  }

  void build(lv_obj_t *parent, JsonObjectConst cfg) override {
    this->root_ = make_tile(parent);
    // Read-only: no click handler. Tile colors come from the active theme (make_tile).

    this->label_ = lv_label_create(this->root_);
    lv_label_set_text(this->label_, (cfg["label"] | "Sensor"));

    this->value_ = lv_label_create(this->root_);
    lv_label_set_text(this->value_, "—");
  }

  void on_state(const EntityState &s) override {
    if (this->root_ == nullptr)
      return;
    std::string unit = s.attr("unit_of_measurement");
    std::string text = s.state;
    if (!unit.empty())
      text += " " + unit;
    lv_label_set_text(this->value_, text.c_str());
  }

  void on_event(lv_event_t *e) override {}  // read-only

 protected:
  lv_obj_t *label_{nullptr};
  lv_obj_t *value_{nullptr};
};

PIXGATE_REGISTER_WIDGET("sensor", SensorWidget);

}  // namespace pixgate
}  // namespace esphome
