#pragma once

// PixGate — shared helpers for widget implementations.
//
// These are small conveniences (tile container creation, color parsing, the tile/detail
// split scaffold from DESIGN.md §10) that keep each widget file focused on its behaviour.

#include "lvgl.h"

#include <cstdlib>
#include <string>

#include "theme.h"
#include "widget.h"

namespace esphome {
namespace pixgate {

// Parse a "#RRGGBB" string into an lv_color_t, falling back to `fallback` on bad input.
inline lv_color_t parse_color(const std::string &hex, lv_color_t fallback) {
  if (hex.size() != 7 || hex[0] != '#')
    return fallback;
  long v = std::strtol(hex.c_str() + 1, nullptr, 16);
  if (v < 0)
    return fallback;
  return lv_color_hex(static_cast<uint32_t>(v));
}

// Apply the active palette (fill, border, text) to a tile-like container so every widget shares
// one look. Widgets that show state through color (switch, light) override bg_color afterwards.
inline void style_tile(lv_obj_t *tile) {
  const Theme &t = active_theme();
  lv_obj_set_style_bg_color(tile, t.widget_bg, 0);
  lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(tile, t.widget_border, 0);
  lv_obj_set_style_border_width(tile, 1, 0);
  lv_obj_set_style_text_color(tile, t.text, 0);
}

// Restyle an already-built widget root into a compact badge: a fully-rounded pill that hugs its
// content, lays its children out in a centered row, and uses the theme's widget colors. Applied
// by the engine to widgets placed in the badge row (DESIGN.md §5).
inline void style_as_badge(lv_obj_t *obj) {
  const Theme &t = active_theme();
  lv_obj_set_size(obj, LV_SIZE_CONTENT, 34);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_pad_top(obj, 4, 0);
  lv_obj_set_style_pad_bottom(obj, 4, 0);
  lv_obj_set_style_pad_left(obj, 14, 0);
  lv_obj_set_style_pad_right(obj, 14, 0);
  lv_obj_set_style_pad_column(obj, 8, 0);  // gap between icon and value
  lv_obj_set_style_bg_color(obj, t.widget_bg, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, t.widget_border, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_text_color(obj, t.text, 0);
  lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// Create a standard square-ish tile container under `parent`. Tiles are the building block
// for simple widgets; complex widgets open a full-screen detail page on tap (§10).
inline lv_obj_t *make_tile(lv_obj_t *parent) {
  lv_obj_t *tile = lv_obj_create(parent);
  lv_obj_set_size(tile, LV_PCT(100), 90);
  lv_obj_set_style_radius(tile, 10, 0);
  lv_obj_set_style_pad_all(tile, 8, 0);
  style_tile(tile);
  lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  return tile;
}

// Route an LVGL event back to the owning Widget. Register with:
//   lv_obj_add_event_cb(obj, &widget_event_trampoline, LV_EVENT_ALL, this);
inline void widget_event_trampoline(lv_event_t *e) {
  auto *w = static_cast<Widget *>(lv_event_get_user_data(e));
  if (w != nullptr)
    w->on_event(e);
}

// Common base providing root-object bookkeeping and a default destroy(). Widgets that build a
// single root LVGL object can inherit this and only implement the behaviour-specific methods.
class TileWidget : public Widget {
 public:
  void destroy() override {
    if (this->root_ != nullptr) {
      lv_obj_del(this->root_);
      this->root_ = nullptr;
    }
  }

 protected:
  lv_obj_t *root_{nullptr};
};

}  // namespace pixgate
}  // namespace esphome
