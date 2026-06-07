#pragma once

// PixGate — responsive layout for the main window (DESIGN.md §6).
//
// The main window arranges entity widgets in a responsive, column-based layout. We use an
// LVGL flex row-wrap container (rather than the grid module, which ESPHome's LVGL build does
// not compile in) and size each child to a fraction of the row, so a widget occupies
// `col_span` of `columns` columns. This keeps the same dashboard readable across boards of
// different resolutions.

#include "lvgl.h"

namespace esphome {
namespace pixgate {

// Where a widget sits in the layout. Stored in the config as cell coordinates + span, never
// as absolute pixels. (col/row are advisory ordering hints; flow order follows the config.)
struct GridCell {
  int col{0};
  int row{0};
  int col_span{1};
  int row_span{1};
};

class GridLayout {
 public:
  // Configure `container` as a responsive row-wrap with `columns` logical columns. Call once
  // after (re)building a page's container.
  void setup(lv_obj_t *container, int columns);

  // Size an already-created child so it spans `cell.col_span` of the configured columns.
  void place(lv_obj_t *child, const GridCell &cell);

  int columns() const { return this->columns_; }

 protected:
  lv_obj_t *container_{nullptr};
  int columns_{4};
};

}  // namespace pixgate
}  // namespace esphome
