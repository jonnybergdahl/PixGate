// PixGate — badge widget (DESIGN.md §5).
//
// A compact status pill for the badge row, bound to a Home Assistant entity. The badge shows an
// optional icon, name and state value (each independently toggleable) and is coloured either by
// the entity's state (highlight when "on"/active, muted when off) or with a fixed colour.
//
// It reports no supported domains, so the web GUI lists it as a system-style widget addable to
// the header/badge zones and lets the entity picker choose any entity. The engine binds it by
// the cfg's entity_id like every other widget.

#include <cstdlib>
#include <cstring>
#include <string>

#include "widget_base.h"
#include "registry.h"
#include "theme.h"

namespace esphome {
namespace pixgate {

// Map a handful of common MDI icon names to LVGL's built-in symbol font. Full MDI rendering needs
// a bundled icon font (DESIGN.md §10) which is not in the build yet; until then unmapped icons
// render nothing rather than tofu, and the show-icon toggle is a no-op for them.
static const char *mdi_to_symbol(const std::string &mdi) {
  struct Map {
    const char *name;
    const char *symbol;
  };
  static const Map MAP[] = {
      {"mdi:home", LV_SYMBOL_HOME},        {"mdi:wifi", LV_SYMBOL_WIFI},
      {"mdi:cog", LV_SYMBOL_SETTINGS},     {"mdi:settings", LV_SYMBOL_SETTINGS},
      {"mdi:power", LV_SYMBOL_POWER},      {"mdi:bell", LV_SYMBOL_BELL},
      {"mdi:battery", LV_SYMBOL_BATTERY_FULL}, {"mdi:bluetooth", LV_SYMBOL_BLUETOOTH},
      {"mdi:usb", LV_SYMBOL_USB},          {"mdi:map-marker", LV_SYMBOL_GPS},
      {"mdi:phone", LV_SYMBOL_CALL},       {"mdi:email", LV_SYMBOL_ENVELOPE},
      {"mdi:image", LV_SYMBOL_IMAGE},      {"mdi:play", LV_SYMBOL_PLAY},
      {"mdi:pause", LV_SYMBOL_PAUSE},      {"mdi:alert", LV_SYMBOL_WARNING},
      {"mdi:eye", LV_SYMBOL_EYE_OPEN},     {"mdi:volume-high", LV_SYMBOL_VOLUME_MAX},
      {"mdi:refresh", LV_SYMBOL_REFRESH},  {"mdi:download", LV_SYMBOL_DOWNLOAD},
      {"mdi:upload", LV_SYMBOL_UPLOAD},    {"mdi:delete", LV_SYMBOL_TRASH},
  };
  for (const Map &m : MAP) {
    if (mdi == m.name)
      return m.symbol;
  }
  return "";
}

// Treat a raw HA state string as "active" for the state-colour mode.
static bool state_is_active(const std::string &state) {
  if (state == "on" || state == "open" || state == "home" || state == "playing" ||
      state == "active" || state == "heat" || state == "cool" || state == "auto" ||
      state == "unlocked" || state == "detected" || state == "true" || state == "yes")
    return true;
  // Numeric states count as active when non-zero (e.g. a power sensor reading).
  char *end = nullptr;
  double v = std::strtod(state.c_str(), &end);
  return end != state.c_str() && v != 0.0;
}

class BadgeWidget : public TileWidget {
 public:
  const char *type_id() const override { return "badge"; }

  // Empty: the badge binds to any entity, and an empty domain list routes it to the header/badge
  // zones in the GUI (and lets the entity picker offer all domains).
  std::vector<std::string> supported_domains() const override { return {}; }

  const ConfigSchema &schema() const override {
    static const ConfigSchema s = {
        {"entity_id", "Entity", ConfigField::ENTITY, {}, "", true},
        {"label", "Name", ConfigField::STRING, {}, "", false},
        {"icon", "Icon", ConfigField::ICON, {}, "mdi:home", false},
        {"show_name", "Show name", ConfigField::BOOL, {}, "true", false},
        {"show_state", "Show state", ConfigField::BOOL, {}, "true", false},
        {"show_icon", "Show icon", ConfigField::BOOL, {}, "true", false},
        {"color_mode", "Colour", ConfigField::ENUM, {"state", "fixed"}, "state", false},
        {"color", "Fixed colour", ConfigField::COLOR, {}, "#4FC3F7", false},
    };
    return s;
  }

  void build(lv_obj_t *parent, JsonObjectConst cfg) override {
    this->show_name_ = cfg["show_name"] | true;
    this->show_state_ = cfg["show_state"] | true;
    this->show_icon_ = cfg["show_icon"] | true;
    this->fixed_ = std::strcmp(cfg["color_mode"] | "state", "fixed") == 0;
    this->color_ = parse_color(cfg["color"] | "#4FC3F7", lv_color_hex(0x4FC3F7));

    this->root_ = lv_obj_create(parent);

    if (this->show_icon_) {
      const char *symbol = mdi_to_symbol(cfg["icon"] | "");
      if (symbol[0] != '\0') {
        this->icon_ = lv_label_create(this->root_);
        lv_label_set_text(this->icon_, symbol);
      }
    }

    if (this->show_name_) {
      const char *name = cfg["label"] | "";
      if (name[0] == '\0')
        name = cfg["entity_id"] | "Badge";
      this->name_ = lv_label_create(this->root_);
      lv_label_set_text(this->name_, name);
    }

    if (this->show_state_) {
      this->value_ = lv_label_create(this->root_);
      lv_label_set_text(this->value_, "—");
    }

    // The engine frames every badge-row widget identically (style_as_badge) after build(); the
    // badge's own colour is layered on top via on_state(), which the engine seeds right after.
  }

  void on_state(const EntityState &s) override {
    if (this->root_ == nullptr)
      return;
    const bool active = s.available && state_is_active(s.state);
    this->apply_colors_(active);

    if (this->value_ != nullptr) {
      if (!s.available || s.state.empty()) {
        lv_label_set_text(this->value_, "—");
      } else {
        std::string text = s.state;
        std::string unit = s.attr("unit_of_measurement");
        if (!unit.empty())
          text += " " + unit;
        lv_label_set_text(this->value_, text.c_str());
      }
    }
  }

  void on_event(lv_event_t *e) override {}  // status-only

 protected:
  // Fixed mode always uses the configured colour. State mode uses it as the "active" highlight
  // and falls back to the theme's muted widget fill when the entity is off/unavailable.
  void apply_colors_(bool active) {
    const bool accent = this->fixed_ || active;
    const lv_color_t bg = accent ? this->color_ : active_theme().widget_bg;
    lv_obj_set_style_bg_color(this->root_, bg, 0);
    // White reads on the saturated accent fill; the theme text colour reads on the muted fill.
    lv_obj_set_style_text_color(this->root_, accent ? lv_color_white() : active_theme().text, 0);
  }

  lv_obj_t *icon_{nullptr};
  lv_obj_t *name_{nullptr};
  lv_obj_t *value_{nullptr};
  bool show_name_{true};
  bool show_state_{true};
  bool show_icon_{true};
  bool fixed_{false};
  lv_color_t color_{};
};

PIXGATE_REGISTER_WIDGET("badge", BadgeWidget);

}  // namespace pixgate
}  // namespace esphome
