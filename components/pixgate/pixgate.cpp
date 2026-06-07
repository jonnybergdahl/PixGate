#include "pixgate.h"

#include <cstring>

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/lvgl/lvgl_esphome.h"

#include "theme.h"

// ArduinoJson (v7) is pulled in via widget.h -> json component; available on both frameworks.

namespace esphome {
namespace pixgate {

static const char *const TAG = "pixgate";
static const char *const PIXGATE_VERSION = "0.1.0";

float PixGate::get_setup_priority() const {
  // Run after the display/lvgl stack is up so the active screen exists.
  return setup_priority::LATE;
}

void PixGate::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PixGate...");
  this->storage_.begin();
  this->config_json_ = this->storage_.load();
  this->rebuild_();
#ifdef USE_OTA_STATE_LISTENER
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif
}

#ifdef USE_OTA_STATE_LISTENER
void PixGate::on_ota_global_state(ota::OTAState state, float progress, uint8_t error,
                                  ota::OTAComponent *component) {
  if (this->lvgl_ == nullptr)
    return;
  switch (state) {
    case ota::OTA_STARTED:
      this->lvgl_->set_paused(true, false);
      break;
    case ota::OTA_ERROR:
    case ota::OTA_ABORT:
      this->lvgl_->set_paused(false, false);
      break;
    default:
      break;
  }
}
#endif

void PixGate::loop() {
  if (this->needs_rebuild_) {
    this->needs_rebuild_ = false;
    this->rebuild_();
  }
}

void PixGate::dump_config() {
  ESP_LOGCONFIG(TAG, "PixGate %s", PIXGATE_VERSION);
  ESP_LOGCONFIG(TAG, "  Config path: %s", this->storage_.path().c_str());
  ESP_LOGCONFIG(TAG, "  Registered widget types: %u",
                static_cast<unsigned>(WidgetRegistry::instance().type_ids().size()));
}

void PixGate::build_root_() {
  lv_obj_t *screen = lv_scr_act();
  if (screen == nullptr) {
    ESP_LOGE(TAG, "No active LVGL screen; is the lvgl: component configured?");
    return;
  }

  // Strip anything already on the active screen. When no pages are configured in YAML, the lvgl:
  // component auto-creates a default "hello world" page (a label plus an animated spinner) on the
  // active screen; the spinner keeps invalidating its region and causes visible flicker behind
  // PixGate's transparent zones. Remove it so only our tree drives the screen.
  lv_obj_clean(screen);

  // Root: full-screen flex column holding the three zones. Transparent — the screen background is
  // painted with the theme color in apply_display_settings_ and shows through behind the tiles.
  this->root_ = lv_obj_create(screen);
  lv_obj_set_size(this->root_, LV_PCT(100), LV_PCT(100));
  lv_obj_center(this->root_);
  lv_obj_set_style_pad_all(this->root_, 0, 0);
  lv_obj_set_style_border_width(this->root_, 0, 0);
  lv_obj_set_style_bg_opa(this->root_, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(this->root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(this->root_, LV_OBJ_FLAG_SCROLLABLE);

  // Header: fixed height, row of system widgets. Filled with the widget background color and
  // separated from the main window by a bottom border in the widget frame color.
  const Theme &th = active_theme();
  this->header_ = lv_obj_create(this->root_);
  lv_obj_set_size(this->header_, LV_PCT(100), 36);
  lv_obj_set_style_pad_all(this->header_, 4, 0);
  lv_obj_set_style_radius(this->header_, 0, 0);
  lv_obj_set_style_bg_color(this->header_, th.widget_bg, 0);
  lv_obj_set_style_bg_opa(this->header_, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(this->header_, th.widget_border, 0);
  lv_obj_set_style_border_width(this->header_, 1, 0);
  lv_obj_set_style_border_side(this->header_, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_flex_flow(this->header_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(this->header_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(this->header_, LV_OBJ_FLAG_SCROLLABLE);

  // Badge row: collapses to zero height when empty.
  this->badges_ = lv_obj_create(this->root_);
  lv_obj_set_width(this->badges_, LV_PCT(100));
  lv_obj_set_height(this->badges_, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_all(this->badges_, 2, 0);
  lv_obj_set_style_border_width(this->badges_, 0, 0);
  lv_obj_set_style_bg_opa(this->badges_, LV_OPA_TRANSP, 0);
  lv_obj_set_flex_flow(this->badges_, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_clear_flag(this->badges_, LV_OBJ_FLAG_SCROLLABLE);

  // Main window: flex-grow, holds the responsive grid of entity widgets.
  this->main_ = lv_obj_create(this->root_);
  lv_obj_set_width(this->main_, LV_PCT(100));
  lv_obj_set_flex_grow(this->main_, 1);
  lv_obj_set_style_pad_all(this->main_, 8, 0);
  lv_obj_set_style_border_width(this->main_, 0, 0);
  lv_obj_set_style_bg_opa(this->main_, LV_OPA_TRANSP, 0);
}

void PixGate::teardown_() {
  // Destroy widget subtrees and detach their bindings.
  for (auto &lw : this->widgets_) {
    if (lw.handle != BindingService::INVALID_HANDLE)
      this->binding_.unsubscribe(lw.handle);
    if (lw.widget)
      lw.widget->destroy();
  }
  this->widgets_.clear();

  // Clear leftover LVGL children of each zone (anything not removed by destroy()).
  if (this->header_ != nullptr)
    lv_obj_clean(this->header_);
  if (this->badges_ != nullptr)
    lv_obj_clean(this->badges_);
  if (this->main_ != nullptr)
    lv_obj_clean(this->main_);
}

void PixGate::attach_widget_(Widget *w) {
  w->set_binding_service(&this->binding_);
}

void PixGate::build_zone_widgets_(lv_obj_t *parent, const void *widgets_array, bool is_grid,
                                  GridLayout *grid) {
  const JsonArrayConst &arr = *static_cast<const JsonArrayConst *>(widgets_array);
  for (JsonObjectConst entry : arr) {
    const char *type = entry["type"] | "";
    if (type[0] == '\0')
      continue;

    auto widget = WidgetRegistry::instance().create(type);
    if (!widget) {
      ESP_LOGW(TAG, "Unknown widget type '%s'; skipping", type);
      continue;
    }

    Widget *raw = widget.get();
    raw->set_instance_id(entry["id"] | "");
    this->attach_widget_(raw);

    JsonObjectConst cfg = entry["cfg"].as<JsonObjectConst>();
    raw->set_entity_id(cfg["entity_id"] | "");
    raw->build(parent, cfg);

    // Position entity widgets in the grid; system widgets just flow in their zone.
    if (is_grid && grid != nullptr) {
      JsonObjectConst cell = entry["cell"].as<JsonObjectConst>();
      GridCell gc;
      gc.col = cell["col"] | 0;
      gc.row = cell["row"] | 0;
      gc.col_span = cell["col_span"] | 1;
      gc.row_span = cell["row_span"] | 1;
      lv_obj_t *child = lv_obj_get_child(parent, lv_obj_get_child_cnt(parent) - 1);
      if (child != nullptr)
        grid->place(child, gc);
    }

    LiveWidget lw;
    lw.widget = std::move(widget);
    this->widgets_.push_back(std::move(lw));
    this->attach_widget_(this->widgets_.back().widget.get());

    // Bind to the entity (subscribe seeds the widget with any cached state immediately).
    Widget *stored = this->widgets_.back().widget.get();
    if (!stored->entity_id().empty()) {
      this->widgets_.back().handle = this->binding_.subscribe(
          stored->entity_id(), [stored](const EntityState &s) { stored->on_state(s); });
    }
  }
}

void PixGate::apply_display_settings_(const void *display_obj) {
  lv_display_t *disp = lv_display_get_default();
  if (disp == nullptr)
    return;
  const JsonObjectConst &d = *static_cast<const JsonObjectConst *>(display_obj);

  // Theme: select our palette and re-init LVGL's default theme (handles control styling for
  // sliders/arcs/buttons) with the matching dark flag. Must run before the widget tree is
  // rebuilt so freshly created tiles read the right palette via style_tile().
  const bool dark = std::strcmp(d["theme"] | "light", "dark") == 0;
  set_active_theme(dark);
  lv_theme_t *theme =
      lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                            dark, LV_FONT_DEFAULT);
  lv_display_set_theme(disp, theme);

  // Paint the screen with our background + text color. The zone containers are transparent
  // (build_root_), so this background shows through behind the tiles. Text color is inherited
  // by labels in the header/badges that aren't inside a tile.
  const Theme &t = active_theme();
  lv_obj_t *screen = lv_display_get_screen_active(disp);
  if (screen != nullptr) {
    lv_obj_set_style_bg_color(screen, t.bg, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, t.text, 0);
  }

  // Orientation: LVGL swaps hor/ver resolution automatically. Note RGB (DPI) panels generally
  // can't rotate at runtime without an extra full framebuffer (DESIGN.md §15) — on those boards
  // this is effectively limited to 0/180. SPI panels rotate cleanly.
  lv_display_rotation_t rot;
  switch (d["orientation"] | 0) {
    case 90:
      rot = LV_DISPLAY_ROTATION_90;
      break;
    case 180:
      rot = LV_DISPLAY_ROTATION_180;
      break;
    case 270:
      rot = LV_DISPLAY_ROTATION_270;
      break;
    default:
      rot = LV_DISPLAY_ROTATION_0;
      break;
  }
  lv_display_set_rotation(disp, rot);
}

void PixGate::rebuild_() {
  if (lv_scr_act() == nullptr) {
    ESP_LOGE(TAG, "No active LVGL screen; is the lvgl: component configured?");
    return;
  }

  this->teardown_();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, this->config_json_);
  if (err) {
    ESP_LOGE(TAG, "Cannot rebuild: config parse error (%s)", err.c_str());
    return;
  }

  // Apply theme + rotation, then rebuild the root tree from scratch so every object picks up the
  // current theme (LVGL only applies a theme to objects at creation time).
  JsonObjectConst display = doc["display"].as<JsonObjectConst>();
  this->apply_display_settings_(&display);
  if (this->root_ != nullptr) {
    lv_obj_del(this->root_);
    this->root_ = this->header_ = this->badges_ = this->main_ = nullptr;
  }
  this->build_root_();
  if (this->root_ == nullptr)
    return;

  // Header system widgets.
  if (doc["header"]["widgets"].is<JsonArrayConst>()) {
    JsonArrayConst arr = doc["header"]["widgets"].as<JsonArrayConst>();
    this->build_zone_widgets_(this->header_, &arr, false, nullptr);
  }

  // Badge row system widgets.
  if (doc["badges"]["widgets"].is<JsonArrayConst>()) {
    JsonArrayConst arr = doc["badges"]["widgets"].as<JsonArrayConst>();
    this->build_zone_widgets_(this->badges_, &arr, false, nullptr);
  }

  // Main window: render the first page (multi-page nav can land later; schema supports it).
  JsonArrayConst pages = doc["pages"].as<JsonArrayConst>();
  if (!pages.isNull() && pages.size() > 0) {
    JsonObjectConst page = pages[0].as<JsonObjectConst>();
    int columns = page["columns"] | 4;
    this->grid_.setup(this->main_, columns);
    if (page["widgets"].is<JsonArrayConst>()) {
      JsonArrayConst arr = page["widgets"].as<JsonArrayConst>();
      this->build_zone_widgets_(this->main_, &arr, true, &this->grid_);
    }
  }

  ESP_LOGI(TAG, "Rebuilt dashboard: %u widgets", static_cast<unsigned>(this->widgets_.size()));
}

bool PixGate::apply_config_json(const std::string &json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    ESP_LOGW(TAG, "Rejecting invalid config (%s)", err.c_str());
    return false;
  }
  // Minimal structural validation: a dashboard must at least have a pages array.
  if (!doc["pages"].is<JsonArrayConst>()) {
    ESP_LOGW(TAG, "Rejecting config: missing 'pages' array");
    return false;
  }

  this->config_json_ = json;
  if (!this->storage_.save(this->config_json_)) {
    ESP_LOGW(TAG, "Config applied in memory but failed to persist");
  }
  // Defer the actual LVGL rebuild to loop() so we never tear down the tree from within an
  // HTTP/event callback that may itself be mid-traversal.
  this->needs_rebuild_ = true;
  return true;
}

std::string PixGate::get_registry_json() {
  JsonDocument doc;
  JsonArray types = doc.to<JsonArray>();
  for (const std::string &type_id : WidgetRegistry::instance().type_ids()) {
    auto widget = WidgetRegistry::instance().create(type_id);
    if (!widget)
      continue;
    JsonObject t = types.add<JsonObject>();
    t["type"] = type_id;

    JsonArray domains = t["domains"].to<JsonArray>();
    for (const std::string &d : widget->supported_domains())
      domains.add(d);

    JsonArray fields = t["schema"].to<JsonArray>();
    for (const ConfigField &f : widget->schema()) {
      JsonObject jf = fields.add<JsonObject>();
      jf["key"] = f.key;
      jf["label"] = f.label;
      jf["type"] = ConfigField::type_to_string(f.type);
      jf["required"] = f.required;
      if (!f.default_value.empty())
        jf["default"] = f.default_value;
      if (!f.options.empty()) {
        JsonArray opts = jf["options"].to<JsonArray>();
        for (const std::string &o : f.options)
          opts.add(o);
      }
    }
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string PixGate::get_device_json() {
  JsonDocument doc;
  doc["version"] = PIXGATE_VERSION;
  doc["name"] = App.get_name();
  lv_disp_t *disp = lv_disp_get_default();
  if (disp != nullptr) {
    doc["width"] = lv_disp_get_hor_res(disp);
    doc["height"] = lv_disp_get_ver_res(disp);
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

}  // namespace pixgate
}  // namespace esphome
