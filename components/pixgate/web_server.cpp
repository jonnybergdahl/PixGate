#include "web_server.h"

#include <algorithm>

#include "esphome/core/log.h"

namespace esphome {
namespace pixgate {

static const char *const TAG = "pixgate.web";

// The widget-setup GUI is a Vite+Svelte SPA hosted on GitHub Pages (see web/). The device serves
// this tiny shell at the root URL: it mounts the SPA into <div id="app"> and pulls the JS/CSS
// bundle from the Pages URL. Because the shell itself is served by the device, the SPA's /api/*
// calls stay same-origin (no CORS needed). The {{SPA_BASE}} token is substituted at request time
// with the configured spa_base_url. If the bundle can't load, the noscript/fallback text shows.
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
  ESP_LOGI(TAG, "PixGate web GUI at /");
}

void PixGateWeb::dump_config() { ESP_LOGCONFIG(TAG, "PixGate Web GUI"); }

bool PixGateWeb::canHandle(AsyncWebServerRequest *request) const {
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const std::string url(request->url_to(buf));
  return url == "/" || url.rfind("/api/", 0) == 0;
}

void PixGateWeb::handleRequest(AsyncWebServerRequest *request) {
  char buf[AsyncWebServerRequest::URL_BUF_SIZE];
  const std::string url(request->url_to(buf));

  if (url == "/") {
    this->handle_spa_(request);
    return;
  }
  if (url == "/api/config") {
    if (request->method() == HTTP_POST) {
      this->handle_post_config_(request);
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

void PixGateWeb::handle_post_config_(AsyncWebServerRequest *request) {
  if (this->pixgate_ == nullptr) {
    request->send(503, "text/plain", "PixGate not ready");
    return;
  }

  // ESPHome's ESP-IDF web server never calls handleBody and doesn't read the body for a JSON
  // POST, so pull it straight off the underlying httpd request here. The implicit
  // AsyncWebServerRequest -> httpd_req_t* conversion gives us the raw request.
  httpd_req_t *req = *request;
  const size_t total = req->content_len;
  std::string body;
  body.reserve(total);
  char buf[512];
  size_t received = 0;
  while (received < total) {
    const size_t want = std::min(sizeof(buf), total - received);
    const int ret = httpd_req_recv(req, buf, want);
    if (ret <= 0) {
      ESP_LOGW(TAG, "Failed to read config body (%d) after %zu/%zu bytes", ret, received, total);
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"truncated body\"}");
      return;
    }
    body.append(buf, ret);
    received += ret;
  }

  if (this->pixgate_->apply_config_json(body)) {
    request->send(200, "application/json", "{\"ok\":true}");
  } else {
    request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid config\"}");
  }
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
