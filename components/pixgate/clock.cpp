// PixGate — clock system widget (DESIGN.md §5).
//
// A system widget for the header zone. It reads the time from a Home Assistant entity (the
// Time & Date integration's `sensor.time`, whose state is already formatted "HH:MM") rather
// than the device clock, so it stays correct even when the panel has no NTP/RTC of its own.

#include "widget_base.h"
#include "registry.h"

namespace esphome {
namespace pixgate {

class ClockWidget : public TileWidget {
 public:
  const char *type_id() const override { return "clock"; }

  // Bound to a HA sensor (defaults to the Time & Date integration's sensor.time).
  std::vector<std::string> supported_domains() const override { return {"sensor"}; }

  const ConfigSchema &schema() const override {
    static const ConfigSchema s = {
        {"entity_id", "Time entity", ConfigField::ENTITY, {}, "sensor.time", false},
    };
    return s;
  }

  void build(lv_obj_t *parent, JsonObjectConst cfg) override {
    // Fall back to sensor.time when no entity was chosen. The engine subscribes from
    // entity_id() after build(), so setting it here is enough to wire the binding.
    if (this->entity_id().empty())
      this->set_entity_id(cfg["entity_id"] | "sensor.time");

    this->root_ = lv_label_create(parent);
    lv_label_set_text(this->root_, "--:--");
  }

  void on_state(const EntityState &s) override {
    if (this->root_ == nullptr)
      return;
    const char *text = (s.available && !s.state.empty()) ? s.state.c_str() : "--:--";
    lv_label_set_text(this->root_, text);
  }

  void on_event(lv_event_t *e) override {}
};

PIXGATE_REGISTER_WIDGET("clock", ClockWidget);

}  // namespace pixgate
}  // namespace esphome
