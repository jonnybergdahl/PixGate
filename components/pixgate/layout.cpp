#include "layout.h"

#include "esphome/core/log.h"

namespace esphome {
namespace pixgate {

static const char *const TAG = "pixgate.layout";

// Horizontal/vertical gap between tiles, in pixels. Kept in sync with the size maths below.
static constexpr int PIXGATE_GAP = 8;

// Minimum touch-target edge for a tile, in pixels. The grid never lets a tile shrink below
// this, so widgets stay finger-sized on every board. This is the single knob that decides how
// many columns/rows a given display gets — e.g. ~5×3 on the 800×480 7" panel. A user-facing
// scale setting (large/small) may layer on top of this later.
static constexpr int PIXGATE_MIN_TILE_PX = 130;

// Fallback main-window dimensions are derived from the display resolution if the measured
// content box looks implausible (layout not yet computed). The display is never narrower than
// this, so anything below it means the measurement is wrong, not the panel.
static constexpr int PIXGATE_MIN_PLAUSIBLE_PX = 120;

void GridLayout::configure(lv_obj_t *main) {
  this->container_ = main;
  // Responsive row-wrap: children flow left-to-right and wrap to new rows as needed. Because we
  // size every tile to an exact pixel width that tiles `columns`-across, wrapping lands exactly
  // `columns` tiles per row on its own.
  lv_obj_set_layout(main, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(main, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(main, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(main, PIXGATE_GAP, 0);
  lv_obj_set_style_pad_column(main, PIXGATE_GAP, 0);

  const int min_touch_px = PIXGATE_MIN_TILE_PX;

  // Measure the real content box. The caller updates the layout first, so this reflects the
  // header/badge heights actually in effect and the current display rotation.
  int w = lv_obj_get_content_width(main);
  int h = lv_obj_get_content_height(main);

  // Guard against a not-yet-computed layout reporting a tiny box (which would collapse the grid
  // to a single column). Fall back to the display resolution minus this object's padding.
  if (w < PIXGATE_MIN_PLAUSIBLE_PX || h < PIXGATE_MIN_PLAUSIBLE_PX) {
    lv_display_t *disp = lv_display_get_default();
    const int pad_x =
        lv_obj_get_style_pad_left(main, LV_PART_MAIN) + lv_obj_get_style_pad_right(main, LV_PART_MAIN);
    const int pad_y =
        lv_obj_get_style_pad_top(main, LV_PART_MAIN) + lv_obj_get_style_pad_bottom(main, LV_PART_MAIN);
    const int disp_w = disp != nullptr ? lv_display_get_horizontal_resolution(disp) : 0;
    const int disp_h = disp != nullptr ? lv_display_get_vertical_resolution(disp) : 0;
    ESP_LOGW(TAG, "main box measured %dx%d (implausible); using display %dx%d", w, h, disp_w,
             disp_h);
    if (w < PIXGATE_MIN_PLAUSIBLE_PX)
      w = disp_w - pad_x;
    if (h < PIXGATE_MIN_PLAUSIBLE_PX)
      h = disp_h - pad_y;
  }
  if (w < 1)
    w = 1;
  if (h < 1)
    h = 1;

  // Largest column/row counts whose resulting tile edge stays >= the minimum, accounting for
  // the inter-tile gaps: n tiles of edge e with (n-1) gaps fit when n*(min+gap) <= box+gap.
  int columns = (w + PIXGATE_GAP) / (min_touch_px + PIXGATE_GAP);
  int rows = (h + PIXGATE_GAP) / (min_touch_px + PIXGATE_GAP);
  if (columns < 1)
    columns = 1;
  if (columns > 16)
    columns = 16;
  if (rows < 1)
    rows = 1;

  GridMetrics m;
  m.columns = columns;
  m.rows = rows;
  // Fill the box: each tile takes an equal share of the remaining space after the gaps. tile_w
  // and tile_h are computed independently, so cells are "roughly square" and fill the screen
  // edge-to-edge rather than leaving dead margin.
  m.tile_w = (w - (columns - 1) * PIXGATE_GAP) / columns;
  m.tile_h = (h - (rows - 1) * PIXGATE_GAP) / rows;
  if (m.tile_w < 1)
    m.tile_w = 1;
  if (m.tile_h < 1)
    m.tile_h = 1;
  this->metrics_ = m;

  ESP_LOGI(TAG, "Grid: box %dx%d, min %d -> %d cols x %d rows, tile %dx%d", w, h, min_touch_px,
           m.columns, m.rows, m.tile_w, m.tile_h);
}

void GridLayout::place(lv_obj_t *child) {
  lv_obj_set_size(child, this->metrics_.tile_w, this->metrics_.tile_h);
  lv_obj_set_flex_grow(child, 0);
}

}  // namespace pixgate
}  // namespace esphome
