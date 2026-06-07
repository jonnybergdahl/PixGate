// PixGate — light widget (DESIGN.md §10).
//
// Tile: tap toggles the light. State drives the on/off colour and an optional brightness
// readout. A long-press is reserved for a future brightness detail page.

#include "widget_base.h"
#include "binding.h"
#include "registry.h"

namespace esphome {
namespace pixgate {

class LightWidget : public TileWidget {
 public:
  const char *type_id() const override { return "light"; }

  std::vector<std::string> supported_domains() const override { return {"light"}; }

  const ConfigSchema &schema() const override {
    static const ConfigSchema s = {
        {"entity_id", "Entity", ConfigField::ENTITY, {}, "", true},
        {"label", "Label", ConfigField::STRING, {}, "", false},
        {"icon", "Icon", ConfigField::ICON, {}, "mdi:lightbulb", false},
        {"color_on", "On colour", ConfigField::COLOR, {}, "#FFD27F", false},
        {"color_off", "Off colour", ConfigField::COLOR, {}, "#333333", false},
    };
    return s;
  }

  void build(lv_obj_t *parent, JsonObjectConst cfg) override {
    this->color_on_ = parse_color(cfg["color_on"] | "#FFD27F", lv_color_hex(0xFFD27F));
    this->color_off_ = parse_color(cfg["color_off"] | "#333333", lv_color_hex(0x333333));

    this->root_ = make_tile(parent);
    lv_obj_set_style_bg_color(this->root_, this->color_off_, 0);
    lv_obj_add_event_cb(this->root_, &widget_event_trampoline, LV_EVENT_CLICKED, this);

    this->label_ = lv_label_create(this->root_);
    lv_label_set_text(this->label_, (cfg["label"] | "Light"));

    this->value_ = lv_label_create(this->root_);
    lv_label_set_text(this->value_, "—");
  }

  void on_state(const EntityState &s) override {
    if (this->root_ == nullptr)
      return;
    const bool on = s.state == "on";
    lv_obj_set_style_bg_color(this->root_, on ? this->color_on_ : this->color_off_, 0);
    // The on-state fill washes out the theme text colour; use the widget border colour so the
    // labels stay legible.
    lv_obj_set_style_text_color(this->root_, on ? active_theme().widget_border : active_theme().text,
                                0);

    std::string brightness = s.attr("brightness");
    if (on && !brightness.empty()) {
      // brightness 0..255 → percentage.
      int b = atoi(brightness.c_str());
      int pct = (b * 100 + 127) / 255;
      lv_label_set_text(this->value_, (std::to_string(pct) + "%").c_str());
    } else {
      lv_label_set_text(this->value_, on ? "On" : "Off");
    }
  }

  void on_event(lv_event_t *e) override {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
      return;
    if (this->binding() == nullptr || this->entity_id().empty())
      return;
    this->binding()->call_service("light", "toggle", {{"entity_id", this->entity_id()}});
  }

 protected:
  lv_obj_t *label_{nullptr};
  lv_obj_t *value_{nullptr};
  lv_color_t color_on_{};
  lv_color_t color_off_{};
};

PIXGATE_REGISTER_WIDGET("light", LightWidget);

}  // namespace pixgate
}  // namespace esphome
