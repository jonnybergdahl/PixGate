#pragma once

// PixGate — device-side web GUI + JSON API (DESIGN.md §11).
//
// Served from the device through ESPHome's web_server_base (shared AsyncWebServer). Exposes:
//   GET  /api/registry  -> available widget types, schemas, supported domains
//   GET  /api/config    -> current dashboard JSON
//   PUT  /api/config    -> replace dashboard JSON (validate, persist, rebuild)
//   GET  /api/device    -> board geometry, version
//   GET  /api/icons     -> available icon names
//   GET  /             -> the configuration SPA (device root)
//
// The handler keeps no HA secrets: entity discovery happens entirely in the browser (§11).

#include <string>

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"

#include "pixgate.h"

namespace esphome {
namespace pixgate {

class PixGateWeb : public Component, public AsyncWebHandler {
 public:
  void set_pixgate(PixGate *pixgate) { this->pixgate_ = pixgate; }
  void set_base(web_server_base::WebServerBase *base) { this->base_ = base; }
  void set_spa_base_url(const std::string &url) { this->spa_base_url_ = url; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // --- AsyncWebHandler ------------------------------------------------------------------
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index,
                  size_t total) override;
  bool isRequestHandlerTrivial() const override { return false; }

 protected:
  void handle_get_config_(AsyncWebServerRequest *request);
  void handle_put_config_(AsyncWebServerRequest *request);
  void handle_registry_(AsyncWebServerRequest *request);
  void handle_device_(AsyncWebServerRequest *request);
  void handle_icons_(AsyncWebServerRequest *request);
  void handle_spa_(AsyncWebServerRequest *request);

  PixGate *pixgate_{nullptr};
  web_server_base::WebServerBase *base_{nullptr};

  // Base URL (no trailing slash) the shell page loads the Svelte GUI bundle from.
  std::string spa_base_url_;

  // Accumulates a PUT body across chunks before applying it.
  std::string body_buffer_;
};

}  // namespace pixgate
}  // namespace esphome
