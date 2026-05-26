#pragma once

#include "esphome/core/component.h"
#include "esphome/components/network/util.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <lvgl.h>
#include <ArduinoJson.h>
#include <string>
#include <vector>

namespace esphome {
namespace pixgate {

class Widget {
 public:
  virtual ~Widget() = default;
  virtual void setup_lvgl(lv_obj_t *parent) = 0;
  virtual void update_state(const std::string &state) = 0;
  std::string entity_id;
  int x, y, w, h;
  lv_obj_t *obj{nullptr};
};

class PixGate : public Component, public web_server::WebHandler, public api::CustomAPIDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_web_server(web_server::WebServer *server) { this->server_ = server; }
  void set_display(display::DisplayBuffer *dis) { this->display_ = dis; }
  void set_rows(int rows) { this->rows_ = rows; }
  void set_columns(int columns) { this->columns_ = columns; }

  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  void save_widgets();
  void load_widgets();
  void add_widget(const std::string &type, const std::string &entity_id, int x, int y);
  void on_state_changed(const std::string &entity_id, const std::string &state);

  web_server::WebServer *server_{nullptr};
  display::DisplayBuffer *display_{nullptr};
  int rows_{2};
  int columns_{2};
  lv_obj_t *grid_{nullptr};
  lv_obj_t *splash_screen_{nullptr};
  lv_obj_t *logo_label_{nullptr};
  lv_obj_t *ip_label_{nullptr};
  bool splash_done_{false};
  uint32_t splash_start_time_{0};
  std::vector<Widget *> widgets_;
};

}  // namespace pixgate
}  // namespace esphome
