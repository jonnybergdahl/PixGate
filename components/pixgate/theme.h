#pragma once

// PixGate — dashboard color palette.
//
// One small palette, selected at rebuild time from the config's `display.theme`. Widgets read
// the active palette through active_theme() (see widget_base.h `style_tile`) so every tile stays
// visually consistent across light/dark without each widget hardcoding colors.

#include "lvgl.h"

namespace esphome {
namespace pixgate {

struct Theme {
  lv_color_t bg;             // screen background
  lv_color_t widget_bg;      // tile/widget container fill
  lv_color_t widget_border;  // tile/widget border
  lv_color_t text;           // primary text
};

// The palette currently in effect. Defaults to the light palette until set_active_theme() runs.
const Theme &active_theme();

// Select light or dark. Called from the engine when applying the config's display settings.
void set_active_theme(bool dark);

}  // namespace pixgate
}  // namespace esphome
