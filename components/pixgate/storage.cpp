#include "storage.h"

#include "esphome/core/log.h"
#include "esphome/components/json/json_util.h"

#ifdef USE_ESP32
#include "nvs.h"
#include "nvs_flash.h"
#endif

namespace esphome {
namespace pixgate {

static const char *const TAG = "pixgate.storage";

// The dashboard document is stored as a single NVS string. NVS is always available on ESP32
// regardless of Arduino/ESP-IDF framework (unlike a LittleFS partition, which needs extra
// partitioning), and commits are atomic — matching the durability goal of DESIGN.md §9.
static const char *const NVS_NAMESPACE = "pixgate";
static const char *const NVS_KEY = "config";

std::string ConfigStorage::default_document_() {
  return std::string("{\"schema_version\":") + std::to_string(PIXGATE_SCHEMA_VERSION) +
         ",\"header\":{\"widgets\":[]},"
         "\"badges\":{\"widgets\":[]},"
         "\"pages\":[{\"name\":\"Home\",\"columns\":4,\"widgets\":[]}]}";
}

bool ConfigStorage::begin() {
#ifdef USE_ESP32
  // ESPHome initialises NVS during boot; calling init again is safe and idempotent.
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  this->mounted_ = err == ESP_OK;
  if (this->mounted_) {
    ESP_LOGI(TAG, "NVS storage ready (namespace '%s')", NVS_NAMESPACE);
  } else {
    ESP_LOGE(TAG, "Failed to init NVS: %d", err);
  }
  return this->mounted_;
#else
  ESP_LOGW(TAG, "Storage backend requires ESP32; using in-memory defaults");
  this->mounted_ = false;
  return false;
#endif
}

std::string ConfigStorage::read_file_(const std::string &path) {
#ifdef USE_ESP32
  if (!this->mounted_)
    return "";
  nvs_handle_t handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
    return "";
  size_t len = 0;
  esp_err_t err = nvs_get_str(handle, NVS_KEY, nullptr, &len);
  if (err != ESP_OK || len == 0) {
    nvs_close(handle);
    return "";
  }
  std::string out(len, '\0');
  err = nvs_get_str(handle, NVS_KEY, &out[0], &len);
  nvs_close(handle);
  if (err != ESP_OK)
    return "";
  // nvs_get_str reports length including the trailing NUL; drop it.
  if (!out.empty() && out.back() == '\0')
    out.pop_back();
  return out;
#else
  return "";
#endif
}

bool ConfigStorage::write_file_(const std::string &path, const std::string &contents) {
#ifdef USE_ESP32
  if (!this->mounted_)
    return false;
  nvs_handle_t handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
    return false;
  esp_err_t err = nvs_set_str(handle, NVS_KEY, contents.c_str());
  if (err == ESP_OK)
    err = nvs_commit(handle);  // atomic: the write is durable only after a successful commit
  nvs_close(handle);
  return err == ESP_OK;
#else
  return false;
#endif
}

std::string ConfigStorage::migrate_(const std::string &json) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    ESP_LOGW(TAG, "Stored config is corrupt (%s); resetting to defaults", err.c_str());
    return default_document_();
  }

  int version = doc["schema_version"] | 0;
  if (version == 0) {
    ESP_LOGW(TAG, "Config missing schema_version; assuming v1");
    version = 1;
  }

  // --- Migration ladder. Each step upgrades exactly one version. -----------------------
  // (No migrations yet; version 1 is current. Add steps here as the schema evolves:)
  //   if (version == 1) { ...transform doc...; version = 2; }

  if (version > PIXGATE_SCHEMA_VERSION) {
    ESP_LOGW(TAG, "Config schema v%d is newer than firmware v%d; using defaults", version,
             PIXGATE_SCHEMA_VERSION);
    return default_document_();
  }

  doc["schema_version"] = PIXGATE_SCHEMA_VERSION;
  std::string out;
  serializeJson(doc, out);
  return out;
}

std::string ConfigStorage::load() {
  std::string raw = this->read_file_(this->path_);
  if (raw.empty()) {
    ESP_LOGI(TAG, "No stored config; creating default dashboard");
    std::string def = default_document_();
    this->save(def);
    return def;
  }
  return this->migrate_(raw);
}

bool ConfigStorage::save(const std::string &json) {
  if (!this->mounted_) {
    ESP_LOGW(TAG, "Cannot save: storage not ready");
    return false;
  }
  if (!this->write_file_(this->path_, json)) {
    ESP_LOGE(TAG, "Failed to persist config");
    return false;
  }
  ESP_LOGI(TAG, "Config saved (%u bytes)", static_cast<unsigned>(json.size()));
  return true;
}

}  // namespace pixgate
}  // namespace esphome
