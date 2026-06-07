// PixGate — climate widget (DESIGN.md §7.4, §10).
//
// This is the deliberately-complex widget the contract is designed against: a card showing
// the current temperature and the setpoint with +/- buttons, plus the current HVAC action.
// Tapping +/- calls climate.set_temperature with the adjusted setpoint.

#include <cmath>
#include <cstdio>

#include "widget_base.h"
#include "binding.h"
#include "registry.h"

namespace esphome {
namespace pixgate {

class ClimateWidget : public TileWidget {
 public:
  const char *type_id() const override { return "climate"; }

  std::vector<std::string> supported_domains() const override { return {"climate"}; }

  const ConfigSchema &schema() const override {
    static const ConfigSchema s = {
        {"entity_id", "Entity", ConfigField::ENTITY, {}, "", true},
        {"label", "Label", ConfigField::STRING, {}, "", false},
        {"icon", "Icon", ConfigField::ICON, {}, "mdi:thermostat", false},
        {"step", "Setpoint step", ConfigField::STRING, {}, "0.5", false},
    };
    return s;
  }

  void build(lv_obj_t *parent, JsonObjectConst cfg) override {
    this->step_ = atof(cfg["step"] | "0.5");
    if (this->step_ <= 0)
      this->step_ = 0.5f;

    // A taller card rather than a square tile (complex widget).
    this->root_ = lv_obj_create(parent);
    lv_obj_set_size(this->root_, LV_PCT(100), 150);
    lv_obj_set_style_radius(this->root_, 10, 0);
    lv_obj_set_style_pad_all(this->root_, 8, 0);
    style_tile(this->root_);  // theme fill/border/text (this card doesn't use make_tile)
    lv_obj_set_flex_flow(this->root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(this->root_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(this->root_, LV_OBJ_FLAG_SCROLLABLE);

    this->label_ = lv_label_create(this->root_);
    lv_label_set_text(this->label_, (cfg["label"] | "Climate"));

    this->current_ = lv_label_create(this->root_);
    lv_label_set_text(this->current_, "—");

    this->action_ = lv_label_create(this->root_);
    lv_label_set_text(this->action_, "");

    // Setpoint control row: [ - ]  setpoint  [ + ]
    lv_obj_t *row = lv_obj_create(this->root_);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    this->minus_ = lv_btn_create(row);
    lv_obj_t *minus_lbl = lv_label_create(this->minus_);
    lv_label_set_text(minus_lbl, "-");
    lv_obj_add_event_cb(this->minus_, &widget_event_trampoline, LV_EVENT_CLICKED, this);

    this->setpoint_ = lv_label_create(row);
    lv_label_set_text(this->setpoint_, "—");

    this->plus_ = lv_btn_create(row);
    lv_obj_t *plus_lbl = lv_label_create(this->plus_);
    lv_label_set_text(plus_lbl, "+");
    lv_obj_add_event_cb(this->plus_, &widget_event_trampoline, LV_EVENT_CLICKED, this);
  }

  void on_state(const EntityState &s) override {
    if (this->root_ == nullptr)
      return;

    std::string cur = s.attr("current_temperature");
    if (!cur.empty())
      lv_label_set_text(this->current_, (std::string("Now: ") + cur + "°").c_str());

    std::string sp = s.attr("temperature");
    if (!sp.empty()) {
      this->setpoint_value_ = atof(sp.c_str());
      this->have_setpoint_ = true;
      this->render_setpoint_();
    }

    std::string action = s.attr("hvac_action");
    if (action.empty())
      action = s.state;  // hvac mode if no action reported
    lv_label_set_text(this->action_, action.c_str());
  }

  void on_event(lv_event_t *e) override {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
      return;
    if (this->binding() == nullptr || this->entity_id().empty() || !this->have_setpoint_)
      return;

    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    if (target == this->minus_)
      this->setpoint_value_ -= this->step_;
    else if (target == this->plus_)
      this->setpoint_value_ += this->step_;
    else
      return;

    this->render_setpoint_();

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", this->setpoint_value_);
    this->binding()->call_service("climate", "set_temperature",
                                  {{"entity_id", this->entity_id()}, {"temperature", buf}});
  }

 protected:
  void render_setpoint_() {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f°", this->setpoint_value_);
    lv_label_set_text(this->setpoint_, buf);
  }

  lv_obj_t *label_{nullptr};
  lv_obj_t *current_{nullptr};
  lv_obj_t *action_{nullptr};
  lv_obj_t *setpoint_{nullptr};
  lv_obj_t *minus_{nullptr};
  lv_obj_t *plus_{nullptr};
  float step_{0.5f};
  float setpoint_value_{20.0f};
  bool have_setpoint_{false};
};

PIXGATE_REGISTER_WIDGET("climate", ClimateWidget);

}  // namespace pixgate
}  // namespace esphome
