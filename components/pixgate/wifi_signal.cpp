// PixGate — wifi signal system widget (DESIGN.md §5).
//
// Header system widget that shows the current WiFi RSSI. Like the clock it refreshes itself
// from device state (ESPHome's wifi component) on a periodic lv_timer, not from HA.

#include "widget_base.h"
#include "registry.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome {
namespace pixgate {

class WifiSignalWidget : public TileWidget {
 public:
  const char *type_id() const override { return "wifi_signal"; }

  std::vector<std::string> supported_domains() const override { return {}; }

  const ConfigSchema &schema() const override {
    static const ConfigSchema s = {};
    return s;
  }

  void build(lv_obj_t *parent, JsonObjectConst cfg) override {
    this->root_ = lv_label_create(parent);
    lv_label_set_text(this->root_, LV_SYMBOL_WIFI " --");
    this->refresh_();
    this->timer_ = lv_timer_create(&WifiSignalWidget::timer_cb_, 5000, this);
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
    static_cast<WifiSignalWidget *>(lv_timer_get_user_data(t))->refresh_();
  }

  void refresh_() {
    if (this->root_ == nullptr)
      return;
#ifdef USE_WIFI
    if (wifi::global_wifi_component != nullptr &&
        wifi::global_wifi_component->is_connected()) {
      int rssi = static_cast<int>(wifi::global_wifi_component->wifi_rssi());
      char buf[24];
      snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %ddBm", rssi);
      lv_label_set_text(this->root_, buf);
      return;
    }
#endif
    lv_label_set_text(this->root_, LV_SYMBOL_WIFI " --");
  }

  lv_timer_t *timer_{nullptr};
};

PIXGATE_REGISTER_WIDGET("wifi_signal", WifiSignalWidget);

}  // namespace pixgate
}  // namespace esphome
