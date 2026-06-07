#include "layout.h"

namespace esphome {
namespace pixgate {

// Horizontal/vertical gap between tiles, in pixels. Kept in sync with the width maths below.
static constexpr int PIXGATE_GAP = 8;

void GridLayout::setup(lv_obj_t *container, int columns) {
  this->container_ = container;
  if (columns < 1)
    columns = 1;
  if (columns > 16)
    columns = 16;
  this->columns_ = columns;

  // Responsive row-wrap: children flow left-to-right and wrap to new rows as needed.
  lv_obj_set_layout(container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(container, PIXGATE_GAP, 0);
  lv_obj_set_style_pad_column(container, PIXGATE_GAP, 0);
}

void GridLayout::place(lv_obj_t *child, const GridCell &cell) {
  int col_span = cell.col_span < 1 ? 1 : cell.col_span;
  if (col_span > this->columns_)
    col_span = this->columns_;

  // Width as a percentage of the row. We reserve a small margin per extra column to leave
  // room for the inter-tile gaps so a full row never overflows and wraps prematurely.
  int pct = (col_span * 100) / this->columns_;
  // Bias down slightly to account for column gaps (cheap, avoids fractional pixel math).
  if (pct > 4)
    pct -= 2;
  if (pct > 100)
    pct = 100;

  lv_obj_set_width(child, LV_PCT(pct));
  lv_obj_set_flex_grow(child, 0);
}

}  // namespace pixgate
}  // namespace esphome
