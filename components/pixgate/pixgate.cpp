#include "pixgate.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/json_helper.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <cstdio>

namespace esphome {
namespace pixgate {

static const char *const TAG = "pixgate";

class TextWidget : public Widget {
 public:
  void setup_lvgl(lv_obj_t *parent) override {
    this->obj = lv_label_create(parent);
    lv_label_set_text(this->obj, "---");
    lv_obj_set_style_text_align(this->obj, LV_TEXT_ALIGN_CENTER, 0);
  }
  void update_state(const std::string &state) override {
    if (this->obj != nullptr) {
      lv_label_set_text(this->obj, state.c_str());
    }
  }
};

void PixGate::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PixGate...");

  // Create Splash Screen
  this->splash_screen_ = lv_obj_create(lv_scr_act());
  lv_obj_set_size(this->splash_screen_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(this->splash_screen_, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(this->splash_screen_, 0, 0);
  lv_obj_set_style_radius(this->splash_screen_, 0, 0);

  this->logo_label_ = lv_label_create(this->splash_screen_);
  lv_label_set_text(this->logo_label_, "PixGate");
  lv_obj_set_style_text_font(this->logo_label_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(this->logo_label_, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(this->logo_label_, LV_ALIGN_CENTER, 0, -20);

  this->ip_label_ = lv_label_create(this->splash_screen_);
  lv_label_set_text(this->ip_label_, "Waiting for network...");
  lv_obj_set_style_text_color(this->ip_label_, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(this->ip_label_, LV_ALIGN_CENTER, 0, 20);

  // Create grid for widgets (initially hidden)
  this->grid_ = lv_obj_create(lv_scr_act());
  lv_obj_set_size(this->grid_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(this->grid_, 0, 0);
  lv_obj_set_style_border_width(this->grid_, 0, 0);
  lv_obj_set_style_radius(this->grid_, 0, 0);
  lv_obj_add_flag(this->grid_, LV_OBJ_FLAG_HIDDEN);

  // Set grid layout
  static lv_coord_t col_dsc[11];
  static lv_coord_t row_dsc[11];

  for (int i = 0; i < this->columns_; i++) col_dsc[i] = LV_GRID_FR(1);
  col_dsc[this->columns_] = LV_GRID_TEMPLATE_LAST;

  for (int i = 0; i < this->rows_; i++) row_dsc[i] = LV_GRID_FR(1);
  row_dsc[this->rows_] = LV_GRID_TEMPLATE_LAST;

  lv_obj_set_layout(this->grid_, LV_LAYOUT_GRID);
  lv_obj_set_grid_dsc_array(this->grid_, col_dsc, row_dsc);

  this->load_widgets();

  if (this->server_ != nullptr) {
    this->server_->add_handler(this);
  }
}

void PixGate::loop() {
  if (this->splash_done_) {
    return;
  }

  if (network::is_connected()) {
    std::string ip = network::get_useable_network().get_ip_address().str();
    lv_label_set_text_fmt(this->ip_label_, "IP: %s", ip.c_str());

    if (this->splash_start_time_ == 0) {
      this->splash_start_time_ = millis();
    }

    // Show splash for at least 3 seconds after getting IP
    if (millis() - this->splash_start_time_ > 3000) {
      lv_obj_add_flag(this->splash_screen_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(this->grid_, LV_OBJ_FLAG_HIDDEN);
      this->splash_done_ = true;
    }
  }
}

void PixGate::on_state_changed(const std::string &entity_id, const std::string &state) {
  ESP_LOGD(TAG, "State changed for %s: %s", entity_id.c_str(), state.c_str());
  for (auto *widget : this->widgets_) {
    if (widget->entity_id == entity_id) {
      widget->update_state(state);
    }
  }
}

void PixGate::handleRequest(AsyncWebServerRequest *request) {
  if (request->url() == "/pixgate") {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>PixGate Config</title>
    <style>
        body { font-family: sans-serif; margin: 20px; }
        .widget { border: 1px solid #ccc; padding: 10px; margin-bottom: 10px; border-radius: 5px; }
        label { display: inline-block; width: 100px; }
    </style>
</head>
<body>
    <h1>PixGate Configuration</h1>
    <div id="widgets"></div>
    <hr>
    <h3>Add Widget</h3>
    <form id="addForm">
        <label>Entity ID:</label> <input type="text" id="entity_id" placeholder="sensor.temperature"><br>
        <label>X:</label> <input type="number" id="x" value="0"><br>
        <label>Y:</label> <input type="number" id="y" value="0"><br>
        <button type="button" onclick="addWidget()">Add</button>
    </form>

    <script>
        function fetchWidgets() {
            fetch('/pixgate/api/widgets').then(r => r.json()).then(data => {
                const container = document.getElementById('widgets');
                container.innerHTML = '<h3>Current Widgets</h3>';
                data.forEach((w, index) => {
                    const div = document.createElement('div');
                    div.className = 'widget';
                    div.innerHTML = `
                        <strong>${w.entity_id}</strong> (X: ${w.x}, Y: ${w.y})
                        <button onclick="deleteWidget(${index})">Delete</button>
                    `;
                    container.appendChild(div);
                });
            }).catch(err => {
                const container = document.getElementById('widgets');
                container.innerHTML = '<p style="color:red">Error fetching widgets. Make sure the device is online.</p>';
            });
        }

        function addWidget() {
            const entity_id = document.getElementById('entity_id').value;
            const x = document.getElementById('x').value;
            const y = document.getElementById('y').value;
            fetch(`/pixgate/api/add?entity_id=${entity_id}&x=${x}&y=${y}`, {method: 'POST'})
                .then(() => {
                    fetchWidgets();
                    document.getElementById('addForm').reset();
                });
        }

        function deleteWidget(index) {
            fetch(`/pixgate/api/delete?index=${index}`, {method: 'POST'})
                .then(() => fetchWidgets());
        }

        fetchWidgets();
    </script>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
  } else if (request->url() == "/pixgate/api/widgets") {
    DynamicJsonDocument doc(2048);
    JsonArray array = doc.to<JsonArray>();
    for (auto *w : this->widgets_) {
      JsonObject obj = array.createNestedObject();
      obj["entity_id"] = w->entity_id;
      obj["x"] = w->x;
      obj["y"] = w->y;
    }
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  } else if (request->url() == "/pixgate/api/add" && request->method() == HTTP_POST) {
    std::string entity_id = request->arg("entity_id").c_str();
    int x = atoi(request->arg("x").c_str());
    int y = atoi(request->arg("y").c_str());
    this->add_widget("text", entity_id, x, y);
    this->save_widgets();
    request->send(200, "text/plain", "OK");
  } else if (request->url() == "/pixgate/api/delete" && request->method() == HTTP_POST) {
    int index = atoi(request->arg("index").c_str());
    if (index >= 0 && index < (int)this->widgets_.size()) {
      delete this->widgets_[index];
      this->widgets_.erase(this->widgets_.begin() + index);
      this->save_widgets();
    }
    request->send(200, "text/plain", "OK");
  }
}

void PixGate::save_widgets() {
  DynamicJsonDocument doc(2048);
  JsonArray array = doc.to<JsonArray>();
  for (auto *w : this->widgets_) {
    JsonObject obj = array.createNestedObject();
    obj["entity_id"] = w->entity_id;
    obj["x"] = w->x;
    obj["y"] = w->y;
  }

  std::string json_data;
  serializeJson(doc, json_data);

  FILE *file = std::fopen("/littlefs/pixgate_widgets.json", "w");
  if (file != nullptr) {
    std::fputs(json_data.c_str(), file);
    std::fclose(file);
    ESP_LOGI(TAG, "Saved widgets to LittleFS");
  } else {
    ESP_LOGE(TAG, "Failed to open file for writing");
  }
}

void PixGate::load_widgets() {
  FILE *file = std::fopen("/littlefs/pixgate_widgets.json", "r");
  if (file == nullptr) {
    ESP_LOGI(TAG, "No widget config found or failed to open");
    return;
  }

  std::fseek(file, 0, SEEK_END);
  long size = std::ftell(file);
  std::fseek(file, 0, SEEK_SET);

  char *buffer = new char[size + 1];
  size_t read_size = std::fread(buffer, 1, size, file);
  buffer[read_size] = '\0';
  std::fclose(file);

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, buffer);
  delete[] buffer;

  if (error) {
    ESP_LOGE(TAG, "JSON parse error: %s", error.c_str());
    return;
  }
  JsonArray array = doc.as<JsonArray>();
  for (JsonObject obj : array) {
    std::string entity_id = obj["entity_id"].as<std::string>();
    int x = obj["x"].as<int>();
    int y = obj["y"].as<int>();
    this->add_widget("text", entity_id, x, y);
  }
  ESP_LOGI(TAG, "Loaded %d widgets", this->widgets_.size());
}

void PixGate::add_widget(const std::string &type, const std::string &entity_id, int x, int y) {
  if (type == "text") {
    auto *w = new TextWidget();
    w->entity_id = entity_id;
    w->x = x;
    w->y = y;
    w->setup_lvgl(this->grid_);
    lv_obj_set_grid_cell(w->obj, LV_GRID_ALIGN_STRETCH, x, 1, LV_GRID_ALIGN_STRETCH, y, 1);
    this->widgets_.push_back(w);
    if (this->is_connected()) {
      this->subscribe_homeassistant_state(&PixGate::on_state_changed, entity_id);
    }
  }
}

void PixGate::dump_config() {
  ESP_LOGCONFIG(TAG, "PixGate:");
  ESP_LOGCONFIG(TAG, "  Rows: %d", this->rows_);
  ESP_LOGCONFIG(TAG, "  Columns: %d", this->columns_);
}

float PixGate::get_setup_priority() const {
  return setup_priority::AFTER_WIFI;
}

}  // namespace pixgate
}  // namespace esphome
