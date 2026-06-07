// PixGate — clock system widget (DESIGN.md §5).
//
// A system widget for the header zone. Unlike entity widgets it has no HA binding; it
// refreshes itself from the device clock via a periodic lv_timer.

#include <ctime>

#include "widget_base.h"
#include "registry.h"

namespace esphome {
namespace pixgate {

class ClockWidget : public TileWidget {
 public:
  const char *type_id() const override { return "clock"; }

  // System widget: not bound to any HA entity domain.
  std::vector<std::string> supported_domains() const override { return {}; }

  const ConfigSchema &schema() const override {
    static const ConfigSchema s = {
        {"format", "Time format", ConfigField::STRING, {}, "%H:%M", false},
    };
    return s;
  }

  void build(lv_obj_t *parent, JsonObjectConst cfg) override {
    this->format_ = cfg["format"] | "%H:%M";
    this->root_ = lv_label_create(parent);
    lv_label_set_text(this->root_, "--:--");
    this->refresh_();
    // Refresh once a second; cheap and keeps the clock current.
    this->timer_ = lv_timer_create(&ClockWidget::timer_cb_, 1000, this);
  }

  void on_state(const EntityState &s) override {}
  void on_event(lv_event_t *e) override {}

  void destroy() override {
    if (this->timer_ != nullptr) {
      lv_timer_del(this->timer_);
      this->timer_ = nullptr;
    }
    TileWidget::destroy();
  }

 protected:
  static void timer_cb_(lv_timer_t *t) {
    static_cast<ClockWidget *>(lv_timer_get_user_data(t))->refresh_();
  }

  void refresh_() {
    if (this->root_ == nullptr)
      return;
    std::time_t now = std::time(nullptr);
    std::tm tm_now{};
    localtime_r(&now, &tm_now);
    char buf[32];
    if (std::strftime(buf, sizeof(buf), this->format_.c_str(), &tm_now) == 0)
      return;
    lv_label_set_text(this->root_, buf);
  }

  std::string format_{"%H:%M"};
  lv_timer_t *timer_{nullptr};
};

PIXGATE_REGISTER_WIDGET("clock", ClockWidget);

}  // namespace pixgate
}  // namespace esphome
