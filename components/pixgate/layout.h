#pragma once

// PixGate — responsive, touch-driven layout for the main window (DESIGN.md §6).
//
// The main window arranges entity widgets in a responsive grid. We use an LVGL flex row-wrap
// container (rather than the grid module, which ESPHome's LVGL build does not compile in).
// Rather than reading a column count from config, the firmware *derives* the column and row
// counts from the main window's real pixel box and a minimum touch-target edge, then sizes
// each tile in explicit pixels to fill the box. This keeps tiles finger-sized and roughly
// square across boards of different resolutions, and is recomputed on every rebuild — so it
// follows the display through runtime rotation for free.

#include "lvgl.h"

namespace esphome {
namespace pixgate {

// Derived grid geometry for the current main-window box. Computed by GridLayout::configure().
struct GridMetrics {
  int columns{1};
  int rows{1};
  int tile_w{0};  // px — each tile fills its column
  int tile_h{0};  // px — each tile fills its row (independent of tile_w → "roughly square")
};

class GridLayout {
 public:
  // Measure `main`'s content box and compute columns/rows/tile size so each tile's edge stays
  // at least the minimum touch-target edge. Also configures `main` as a responsive row-wrap.
  // Call after the header and badge zones are populated and the layout has been updated, so the
  // measured box reflects the real available area (and the current rotation).
  void configure(lv_obj_t *main);

  // Size an already-created child to one grid cell (tile_w × tile_h). Spans are not yet honored
  // — every widget occupies a single square-ish cell for v1.
  void place(lv_obj_t *child);

  const GridMetrics &metrics() const { return this->metrics_; }
  int columns() const { return this->metrics_.columns; }

 protected:
  lv_obj_t *container_{nullptr};
  GridMetrics metrics_{};
};

}  // namespace pixgate
}  // namespace esphome
