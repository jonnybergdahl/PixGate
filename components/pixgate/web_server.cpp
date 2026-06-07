#include "web_server.h"

#include "esphome/core/log.h"

namespace esphome {
namespace pixgate {

static const char *const TAG = "pixgate.web";

// The widget-setup GUI is a Vite+Svelte SPA hosted on GitHub Pages (see web/). The device only
// serves this tiny shell: it mounts the SPA into <div id="app"> and pulls the JS/CSS bundle
// from the Pages URL. Because the shell itself is served by the device, the SPA's /api/* calls
// stay same-origin (no CORS needed). The {{SPA_BASE}} token is substituted at request time with
// the configured spa_base_url. If the bundle can't load, the noscript/fallback text shows.
static const char PIXGATE_SHELL[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PixGate</title>
<link rel="stylesheet" href="{{SPA_BASE}}/pixgate.css">
<style>body{margin:0;font-family:system-ui,sans-serif}#app:empty::after{content:"Loading PixGate GUI from {{SPA_BASE}} \2026 If this persists, check the device's internet access or the configured spa_base_url.";display:block;padding:1.5rem;color:#666}</style>
</head><body>
<div id="app"></div>
<script type="module" src="{{SPA_BASE}}/pixgate.js"></script>
<noscript>PixGate's configuration GUI requires JavaScript.</noscript>
</body></html>)HTML";

void PixGateWeb::setup() {
  if (this->base_ == nullptr) {
    ESP_LOGE(TAG, "No web_server_base configured");
    return;
  }
  this->base_->init();
  this->base_->add_handler(this);
  ESP_LOGI(TAG, "PixGate web GUI at /pixgate");
}

void PixGateWeb::dump_config() { ESP_LOGCONFIG(TAG, "PixGate Web GUI"); }

bool PixGateWeb::canHandle(AsyncWebServerRequest *request) const {
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const std::string url(request->url_to(buf));
  return url == "/pixgate" || url.rfind("/api/", 0) == 0;
}

void PixGateWeb::handleRequest(AsyncWebServerRequest *request) {
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const std::string url(request->url_to(buf));

  if (url == "/pixgate") {
    this->handle_spa_(request);
    return;
  }
  if (url == "/api/config") {
    if (request->method() == HTTP_PUT) {
      this->handle_put_config_(request);
    } else {
      this->handle_get_config_(request);
    }
    return;
  }
  if (url == "/api/registry") {
    this->handle_registry_(request);
    return;
  }
  if (url == "/api/device") {
    this->handle_device_(request);
    return;
  }
  if (url == "/api/icons") {
    this->handle_icons_(request);
    return;
  }
  request->send(404, "text/plain", "Not found");
}

void PixGateWeb::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len,
                            size_t index, size_t total) {
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const std::string url(request->url_to(buf));
  if (url != "/api/config" || request->method() != HTTP_PUT)
    return;
  if (index == 0)
    this->body_buffer_.clear();
  this->body_buffer_.append(reinterpret_cast<const char *>(data), len);
  // handleRequest() runs after the body is fully received and consumes body_buffer_.
}

void PixGateWeb::handle_spa_(AsyncWebServerRequest *request) {
  std::string html(PIXGATE_SHELL);
  // Substitute every {{SPA_BASE}} occurrence with the configured bundle base URL.
  const std::string token = "{{SPA_BASE}}";
  for (size_t pos = html.find(token); pos != std::string::npos; pos = html.find(token, pos)) {
    html.replace(pos, token.size(), this->spa_base_url_);
    pos += this->spa_base_url_.size();
  }
  request->send(200, "text/html", html.c_str());
}

void PixGateWeb::handle_get_config_(AsyncWebServerRequest *request) {
  if (this->pixgate_ == nullptr) {
    request->send(503, "text/plain", "PixGate not ready");
    return;
  }
  request->send(200, "application/json", this->pixgate_->get_config_json().c_str());
}

void PixGateWeb::handle_put_config_(AsyncWebServerRequest *request) {
  if (this->pixgate_ == nullptr) {
    request->send(503, "text/plain", "PixGate not ready");
    return;
  }
  if (this->pixgate_->apply_config_json(this->body_buffer_)) {
    request->send(200, "application/json", "{\"ok\":true}");
  } else {
    request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid config\"}");
  }
  this->body_buffer_.clear();
}

void PixGateWeb::handle_registry_(AsyncWebServerRequest *request) {
  if (this->pixgate_ == nullptr) {
    request->send(503, "text/plain", "PixGate not ready");
    return;
  }
  request->send(200, "application/json", this->pixgate_->get_registry_json().c_str());
}

void PixGateWeb::handle_device_(AsyncWebServerRequest *request) {
  if (this->pixgate_ == nullptr) {
    request->send(503, "text/plain", "PixGate not ready");
    return;
  }
  request->send(200, "application/json", this->pixgate_->get_device_json().c_str());
}

void PixGateWeb::handle_icons_(AsyncWebServerRequest *request) {
  // The bundled MDI subset is a follow-up (DESIGN.md §10 / §15). Report an empty set for now;
  // the GUI falls back to manual icon-name entry until the font subset lands.
  request->send(200, "application/json", "[]");
}

}  // namespace pixgate
}  // namespace esphome
