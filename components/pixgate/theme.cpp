#include "theme.h"

namespace esphome {
namespace pixgate {

// Light palette (user-specified).
static const Theme LIGHT_THEME = {
    lv_color_hex(0xfafafa),  // bg
    lv_color_hex(0xffffff),  // widget_bg
    lv_color_hex(0xefefef),  // widget_border
    lv_color_hex(0x141414),  // text
};

// Dark palette (user-specified).
static const Theme DARK_THEME = {
    lv_color_hex(0x111111),  // bg
    lv_color_hex(0x1c1c1c),  // widget_bg
    lv_color_hex(0x272727),  // widget_border
    lv_color_hex(0xe1e1e1),  // text
};

static Theme g_active = LIGHT_THEME;

const Theme &active_theme() { return g_active; }

void set_active_theme(bool dark) { g_active = dark ? DARK_THEME : LIGHT_THEME; }

}  // namespace pixgate
}  // namespace esphome
