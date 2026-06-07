// PixGate — switch widget (DESIGN.md §10).
//
// Tile: tap toggles a switch or input_boolean. Trivial on/off, intentionally minimal.

#include "widget_base.h"
#include "binding.h"
#include "registry.h"

namespace esphome {
namespace pixgate {

class SwitchWidget : public TileWidget {
 public:
  const char *type_id() const override { return "switch"; }

  std::vector<std::string> supported_domains() const override {
    return {"switch", "input_boolean"};
  }

  const ConfigSchema &schema() const override {
    static const ConfigSchema s = {
        {"entity_id", "Entity", ConfigField::ENTITY, {}, "", true},
        {"label", "Label", ConfigField::STRING, {}, "", false},
        {"icon", "Icon", ConfigField::ICON, {}, "mdi:toggle-switch", false},
    };
    return s;
  }

  void build(lv_obj_t *parent, JsonObjectConst cfg) override {
    this->root_ = make_tile(parent);
    lv_obj_set_style_bg_color(this->root_, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(this->root_, &widget_event_trampoline, LV_EVENT_CLICKED, this);

    this->label_ = lv_label_create(this->root_);
    lv_label_set_text(this->label_, (cfg["label"] | "Switch"));

    this->value_ = lv_label_create(this->root_);
    lv_label_set_text(this->value_, "—");

    // Remember the entity's domain so the service call targets the right one.
    std::string id = cfg["entity_id"] | "";
    this->is_input_boolean_ = id.rfind("input_boolean.", 0) == 0;
  }

  void on_state(const EntityState &s) override {
    if (this->root_ == nullptr)
      return;
    const bool on = s.state == "on";
    lv_obj_set_style_bg_color(this->root_, on ? lv_color_hex(0x2E7D32) : lv_color_hex(0x333333),
                              0);
    // The on-state fill washes out the theme text colour; use the widget border colour so the
    // labels stay legible.
    lv_obj_set_style_text_color(this->root_, on ? active_theme().widget_border : active_theme().text,
                                0);
    lv_label_set_text(this->value_, on ? "On" : "Off");
  }

  void on_event(lv_event_t *e) override {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
      return;
    if (this->binding() == nullptr || this->entity_id().empty())
      return;
    const char *domain = this->is_input_boolean_ ? "input_boolean" : "switch";
    this->binding()->call_service(domain, "toggle", {{"entity_id", this->entity_id()}});
  }

 protected:
  lv_obj_t *label_{nullptr};
  lv_obj_t *value_{nullptr};
  bool is_input_boolean_{false};
};

PIXGATE_REGISTER_WIDGET("switch", SwitchWidget);

}  // namespace pixgate
}  // namespace esphome
