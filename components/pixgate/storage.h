#pragma once

// PixGate — configuration storage (DESIGN.md §9).
//
// The whole dashboard lives on-device as a single versioned JSON document. The backend is
// ESP32 NVS (a single string entry): it is always available regardless of the Arduino /
// ESP-IDF framework split and needs no custom flash partitioning. A `schema_version` is
// present from day one and migrations upgrade older documents on load. Each save is a single
// NVS commit, which is atomic, so a power loss mid-save cannot corrupt the live config.

#include <string>

namespace esphome {
namespace pixgate {

// The schema version this firmware writes. Bump when the document shape changes and add a
// corresponding step to ConfigStorage::migrate_().
static constexpr int PIXGATE_SCHEMA_VERSION = 1;

class ConfigStorage {
 public:
  void set_path(const std::string &path) { this->path_ = path; }
  const std::string &path() const { return this->path_; }

  // Mount the underlying filesystem. Returns false if mounting failed.
  bool begin();

  // Load the dashboard JSON document. If no document exists (or it is corrupt) returns a
  // freshly created default document. The returned string is always a valid JSON object,
  // already migrated to PIXGATE_SCHEMA_VERSION.
  std::string load();

  // Persist the dashboard JSON document atomically. The document is expected to be valid
  // JSON; `schema_version` is forced to PIXGATE_SCHEMA_VERSION before writing.
  bool save(const std::string &json);

 protected:
  // Read the raw file contents (empty if missing).
  std::string read_file_(const std::string &path);
  // Write contents to a file, truncating it.
  bool write_file_(const std::string &path, const std::string &contents);

  // Upgrade an older document in place to the current schema version. Returns the migrated
  // JSON string.
  std::string migrate_(const std::string &json);

  // The minimal valid empty dashboard.
  static std::string default_document_();

  std::string path_{"/pixgate.json"};
  bool mounted_{false};
};

}  // namespace pixgate
}  // namespace esphome
