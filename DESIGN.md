# DESIGN.md — ESPHome LVGL Dashboard Library

> **Working name:** `espdash` (placeholder — rename the component directory and C++
> namespace consistently if you choose a different name).
> **License:** MIT.
> **Status:** Initial design. This document is the source of truth for architecture and
> the rationale behind each decision. If you change a decision, update this file and note
> *why*, because several choices here exist specifically to avoid known dead ends.

---

## 1. What this is

A reusable **ESPHome external component** that turns a PSRAM-equipped ESP32 touchscreen dev
board into a Home Assistant control panel. The user builds their dashboard entirely in a
**web GUI served by the device** — choosing widgets, binding them to Home Assistant
entities, and arranging them on screen. No dashboard configuration ever appears in YAML.

Two consumption paths, both first-class:

1. **Library path (tinkerers):** the user adds an `external_components:` block plus a small
   standard device YAML (display + touch + the usual wifi/api boilerplate), compiles, and
   flashes. They then configure everything in the web GUI.
2. **Precompiled path (everyone):** prebuilt firmware images per supported board, hosted on
   GitHub, flashable from the browser via ESP Web Tools, then configured in the web GUI.

The defining principle: **everything visible on the display is set up in the web GUI at
runtime.** The only YAML is the ESPHome device definition that decides what to compile.

---

## 2. Goals and non-goals

### Goals
- Drop-in: a beginner copies a short YAML snippet, flashes, and has a working panel.
- Board-agnostic: the device YAML defines the display + touch; the library adapts to
  whatever LVGL display ESPHome set up. No per-board code in the engine.
- Runtime-configurable: add/remove/rearrange/rebind widgets from the web GUI with no
  recompile and no reflash.
- Extensible: adding a new widget type is one new C++ file + registration, with the web GUI
  picking it up automatically via a schema.
- On-device config: the dashboard definition lives on the device.

### Non-goals (explicitly out of scope)
- **Low-memory boards.** We target boards with PSRAM only. Do not compromise the design to
  fit ESP32 classic / sub-PSRAM parts.
- **YAML-defined widgets.** espcontrol-style YAML widget definitions are explicitly *not*
  wanted. The engine is native C++; YAML is only the ESPHome device file.
- **MQTT.** We use ESPHome's native Home Assistant API, not MQTT.
- **Driving the display ourselves.** We build on ESPHome's `lvgl:` component (see §4).

---

## 3. Target hardware

Test boards (all have extra PSRAM):
- **Guition JC8048W550** (ESP32-S3, 4.3–5" 480×... class panel)
- **Guition JC827W543** (ESP32-S3)
- **WT32-SC01 Plus** (ESP32-S3, 320×480)

Note: espcontrol (the closest prior art) was confirmed to run on the WT32-SC01 with no
issues, which validates that this class of board has enough headroom for a runtime LVGL
widget engine.

The engine must not hard-code resolution, controller, rotation, or touch chip. It reads the
active LVGL display's geometry at runtime and lays out accordingly.

---

## 4. Core architectural decision: build *on top of* ESPHome's `lvgl:` component

**Decision:** ESPHome's `lvgl:` component owns all hardware glue — LVGL init, display flush
callbacks, DMA, PSRAM draw buffers, touch input, and pumping `lv_timer_handler()` on the
main loop. Our component owns **only the runtime widget tree** built on top of that, using
the raw `lv_obj_*` C API.

**Why:**
- Hardware abstraction comes for free. Our engine needs *no* display or touchscreen
  references; it operates on whatever LVGL display the `lvgl:` component configured. This is
  what makes "the device YAML just defines display + touch and it all works" true.
- Avoids reimplementing the flush layer per display controller — a maintenance sinkhole
  across current and future boards.
- LVGL is a single global instance and ESPHome is single-threaded (everything runs on the
  main loop). All our `lv_*` calls happen on the main loop, so there are **no locking or
  threading concerns** as long as we never call LVGL from another task/ISR.

**Consequence / the one real risk to validate first (see §13):** we create and destroy LVGL
objects *at runtime* on the active display, while the `lvgl:` component continues to manage
that same display. This should be fine (one LVGL instance), but the modern `lvgl:` component
(ESPHome 2025.x) has its own lifecycle opinions. **Spike this before building anything
else.** espcontrol already solved this exact binding — read its `components/espcontrol`
source to confirm the working pattern.

The minimal device YAML therefore contains: board/framework, `display:`, `touchscreen:`, a
minimal `lvgl:` stub, the `external_components:` pointing at this repo, and the
wifi/api/ota/provisioning boilerplate (shipped as a per-board base config — see §11/§12).

---

## 5. Screen structure

A single root screen composed of three vertical zones (LVGL flex column):

```
┌─────────────────────────────────────────────┐
│  HEADER (fixed height)                        │  time, wifi signal, status icons
├─────────────────────────────────────────────┤
│  BADGE ROW (optional, collapsible)            │  compact status badges
├─────────────────────────────────────────────┤
│                                               │
│  MAIN WINDOW (flex-grow)                      │  entity widgets, laid out by §6
│                                               │
└─────────────────────────────────────────────┘
```

- **Header** and **badge row** host *system widgets* (time, wifi RSSI, etc.). The badge row
  is optional and collapses to zero height when empty.
- **Main window** hosts *entity widgets* and contains the layout engine (§6).
- All three zones are configured through the same web GUI mechanism; the distinction is
  only which zone a widget may live in.

Keep the **system widget vs entity widget** boundary explicit in code — they share the
`Widget` contract (§7) but are placed and constrained differently.

---

## 6. Layout engine (main window)

The main window uses a **responsive grid** (LVGL grid layout):
- A configurable column count; the grid adapts cell size to the active display width so the
  same dashboard looks reasonable across boards of different resolutions. This responsiveness
  is the main differentiator vs absolute-coordinate systems (e.g. openHASP).
- Each widget occupies a grid cell with an optional **column/row span**, allowing
  mixed-size widgets (a tall climate card next to a small toggle).
- The web GUI provides drag-and-drop placement; placement is stored as cell coordinates +
  span, not absolute pixels.

**Pages:** design the data model for **multiple pages** in the main window from day one
(a `pages` array — see §9), with a page indicator and swipe navigation. v1 may render only
the first page, but the schema must not need migration to add pages later.

---

## 7. Widget architecture (the core of the project)

Everything hinges on a **small, stable widget contract** plus a **registry** plus a
**self-describing config schema**. The schema is what lets the web GUI render a config form
per widget type without the GUI hard-coding knowledge of each type — this is what keeps
"easy to add new widget types" actually true.

### 7.1 Contract

```cpp
namespace esphome {
namespace espdash {

// A normalized snapshot of a Home Assistant entity's state, delivered to widgets.
struct EntityState {
  std::string entity_id;
  std::string state;                               // raw state string ("on", "23.4", ...)
  std::map<std::string, std::string> attributes;   // e.g. brightness, temperature, hvac_action
  bool available{false};
};

// Describes one configurable field of a widget type. Serialized to JSON and sent to the
// web GUI so it can render the correct form control. Drives "add a widget type = one file".
struct ConfigField {
  std::string key;            // json key in the widget config
  std::string label;          // human label in the GUI
  enum Type { ENTITY, STRING, BOOL, INT, ENUM, COLOR, ICON } type;
  std::vector<std::string> options;  // for ENUM
  std::string default_value;
  bool required{false};
};
using ConfigSchema = std::vector<ConfigField>;

class Widget {
 public:
  virtual ~Widget() = default;

  // Identity — must be globally unique and stable (it is stored in saved configs).
  virtual const char *type_id() const = 0;          // "light", "switch", "sensor", "climate"

  // What entity domain(s) this widget can bind to (used to filter the GUI entity picker).
  virtual std::vector<std::string> supported_domains() const = 0;

  // Construct the LVGL subtree under `parent` from this widget's slice of config.
  virtual void build(lv_obj_t *parent, const JsonObjectConst &cfg) = 0;

  // Apply a new HA state to the visuals. Called whenever the bound entity changes.
  virtual void on_state(const EntityState &s) = 0;

  // Handle an LVGL event (touch/slider/etc.) and translate it to an intent (service call
  // via the binding service in §8). Registered as the lv_event_cb during build().
  virtual void on_event(lv_event_t *e) = 0;

  // Self-description for the web GUI form.
  virtual const ConfigSchema &schema() const = 0;

  // Teardown: delete LVGL objects this widget created. Called on rebuild/remove.
  virtual void destroy() = 0;
};

}  // namespace espdash
}  // namespace esphome
```

### 7.2 Registry

Widget types self-register at static-init via a macro, so the engine can instantiate by
`type_id` from saved config and the GUI can enumerate available types + schemas.

```cpp
// In each widget's .cpp:
ESPDASH_REGISTER_WIDGET("light", LightWidget);
```

The macro inserts a factory (`std::function<std::unique_ptr<Widget>()>`) into a global
registry map keyed by `type_id`. The engine exposes the registry (type_id + schema +
supported_domains) over the JSON API (§11) so the GUI is always in sync with the firmware.

### 7.3 Lifecycle

- **Boot:** load config JSON (§9) → for each widget entry, look up factory by `type_id`,
  instantiate, `build()` into its zone/cell, then `attach` to the binding service (§8) for
  its `entity_id`.
- **Config change from GUI:** simplest correct approach for v1 is **teardown + rebuild** the
  affected zone (call `destroy()` on its widgets, detach bindings, rebuild from new config).
  Diffing can come later as an optimization; do not start with it.
- **State update:** binding service invokes the widget's `on_state()`.

### 7.4 Design the contract against `climate`, not `switch`

`switch` is trivial and will mislead the abstraction. `climate` is the first genuinely
complex widget (hvac mode + target setpoint + current temperature + hvac action + step), and
designing the contract so climate fits cleanly will prevent an early refactor.

---

## 8. Runtime Home Assistant binding (the quiet enabler)

Because entities are chosen in the GUI at runtime, we **cannot** use ESPHome's compile-time
`homeassistant:` sensor/text_sensor/service entities — those require entity IDs known at
build time. We need to **subscribe to arbitrary entity states** and **call services with
arbitrary entity_ids** dynamically.

**Build a `BindingService`** that wraps ESPHome's native API client layer:
- `subscribe(entity_id, callback)` / `unsubscribe(entity_id, callback)` — dedupe multiple
  widgets bound to the same entity behind a single underlying subscription.
- `call_service(domain, service, data)` — e.g. `light.turn_on`, `climate.set_temperature`.
- Maintains an `entity_id -> latest EntityState` cache so a freshly built widget can be
  seeded immediately.

**Integration points to verify against the pinned ESPHome version** (names may differ; grep
the ESPHome source — do not assume):
- State subscription: ESPHome's API server has a Home Assistant state subscription mechanism
  (the `homeassistant` platforms use it internally — look for
  `subscribe_home_assistant_state` / `subscribe_homeassistant_state` on the API server and
  how attributes are requested).
- Service calls: the device can send a Home Assistant service call over the native API
  (look for `send_homeassistant_service_call` / `HomeassistantServiceCall` on
  `api::global_api_server`).
- Requires `api:` configured and HA's ESPHome integration set to **allow the device to
  perform actions / make service calls** (the `actions:`/services capability on the API).

This service is small but it is the linchpin that makes web-configuration possible. Get it
right and tested early (a hardcoded subscribe + a hardcoded `light.toggle` is a fine spike).

---

## 9. Configuration storage

- **Location:** on-device flash, a small **LittleFS** partition. Boards have ample flash.
- **Format:** a single versioned JSON document (the whole dashboard).
- **`schema_version`** is present from day one. Write a migration step that upgrades older
  documents on load. The cost of adding it now is near zero; retrofitting it later is painful.
- **Atomic writes:** write to a temp file then rename, so a power loss mid-save cannot
  corrupt the live config.

### Example document shape

```json
{
  "schema_version": 1,
  "header": {
    "widgets": [
      { "type": "clock", "cfg": {} },
      { "type": "wifi_signal", "cfg": {} }
    ]
  },
  "badges": { "widgets": [] },
  "pages": [
    {
      "name": "Home",
      "columns": 4,
      "widgets": [
        {
          "id": "w1",
          "type": "light",
          "cell": { "col": 0, "row": 0, "col_span": 1, "row_span": 1 },
          "cfg": {
            "entity_id": "light.living_room",
            "label": "Living Room",
            "icon": "mdi:sofa",
            "color_on": "#FFD27F",
            "color_off": "#333333"
          }
        },
        {
          "id": "w2",
          "type": "climate",
          "cell": { "col": 1, "row": 0, "col_span": 2, "row_span": 2 },
          "cfg": { "entity_id": "climate.lounge", "label": "Lounge" }
        }
      ]
    }
  ]
}
```

`cfg` is the per-widget slice handed to `Widget::build()` and validated against the widget's
`ConfigSchema`.

---

## 10. Starter widget set (v1)

Implement exactly these four first, then stop and validate the abstraction before adding more:

| type     | domain(s)            | reads (state/attrs)                          | writes (service)                                  | UI |
|----------|----------------------|----------------------------------------------|---------------------------------------------------|----|
| `light`  | `light`              | on/off, `brightness`                         | `light.toggle`, `light.turn_on` (brightness)      | tile; tap=toggle; long-press/detail=brightness |
| `switch` | `switch`, `input_boolean` | on/off                                  | `switch.toggle`                                   | tile; tap=toggle |
| `sensor` | `sensor`, `binary_sensor` | value + `unit_of_measurement`           | none (read-only)                                  | value tile |
| `climate`| `climate`            | `hvac_action`, `current_temperature`, `temperature` (setpoint), `hvac_modes`, mode | `climate.set_temperature`, `climate.set_hvac_mode` | card: current + setpoint +/- + mode |

Notes:
- **Icons:** auto-pick a default icon by domain; allow manual override (MDI icon set). Bundle
  an MDI subset as an LVGL font + a name→codepoint map exposed to the GUI.
- **Tile vs detail:** simple types are a single tile. `climate` (and future complex types like
  `media_player`, `cover`) want a full-screen **detail page** opened on tap; bake the
  tile/detail split into the widget pattern now so complex widgets fit without a redesign.

### Adding a new widget type later (the intended workflow)
1. New file `components/espdash/widgets/<type>.cpp/.h` subclassing `Widget`.
2. Implement `type_id`, `supported_domains`, `build`, `on_state`, `on_event`, `schema`,
   `destroy`.
3. `ESPDASH_REGISTER_WIDGET("<type>", <Class>);`
4. Done — the GUI discovers it via the registry/schema API; no GUI code change required.

---

## 11. Web GUI

- **Served from the device.** Build the GUI as a separate small SPA, then embed it in the
  firmware as a **gzipped static asset** served by an HTTP handler. Keep the SPA tiny.
- **JSON API** (device side), roughly:
  - `GET /api/registry` → available widget types, their schemas, supported domains.
  - `GET /api/config` → current dashboard JSON.
  - `POST /api/config` → replace dashboard JSON (validate, persist atomically, trigger rebuild).
    (POST, not PUT: ESPHome's ESP-IDF web server only registers GET/POST/OPTIONS handlers.)
  - `GET /api/icons` → available icon names.
  - `GET /api/device` → board geometry, version, etc.
- **Live apply:** on `POST /api/config`, persist then rebuild affected zones so the change is
  visible on the panel immediately.

### The entity-picker problem (decide deliberately)
The device does **not** know Home Assistant's full entity list, so it cannot offer a nice
entity dropdown on its own. Chosen approach:
- The **browser** (already on the same LAN as HA) fetches the entity list directly from Home
  Assistant. The user supplies their HA base URL + a long-lived access token in the GUI; the
  browser calls HA's REST/WebSocket API, filters by the widget's `supported_domains()`, and
  writes the chosen `entity_id` into the device config.
- **Fallback:** manual `entity_id` text entry (always available, no token required).
- The HA URL/token are used **by the browser only** for entity discovery; storing them on the
  device is optional and should be opt-in. This is the only part of the system that reaches
  outside the device — keep it isolated and clearly documented.

---

## 12. Provisioning (required for precompiled images)

Precompiled firmware ships without the user's WiFi, so it needs a first-boot story. Include
in the per-board base YAML:
- `wifi:` with an **`ap:` fallback** + **`captive_portal:`** — if it can't join a network it
  raises its own AP so the user enters WiFi credentials from a phone/laptop.
- **`improv_serial:`** so ESP Web Tools can set WiFi during the browser flash.
- Host an **ESP Web Tools manifest** on the GitHub Pages site for one-click browser flashing
  per board.
- HA auto-discovery via the native `api:` so the device appears in Home Assistant after it
  joins WiFi.

### API encryption wrinkle (don't get caught by this)
A single generic precompiled image cannot embed a per-user API encryption key. Options:
- Ship precompiled images with **API encryption disabled** (relying on LAN trust), and let
  users enable encryption when they adopt/recompile; **or**
- Generate/install an encryption key during provisioning.
Document whichever you pick. For v1, unencrypted-on-LAN is the pragmatic default for the
precompiled path; the library path can use encryption normally.

---

## 13. First milestone — the LVGL spike (do this before anything else)

The single highest-leverage validation. Everything else is comparatively conventional; this
is the one place the whole architecture could need to bend.

**Spike goal:** on one test board, with ESPHome's `lvgl:` component active, prove you can:
1. Create LVGL objects at runtime (`lv_obj_create`, a button, a label) on the active display,
   after boot, not from YAML.
2. Receive a touch event on that runtime-created object.
3. Destroy those objects at runtime and create different ones — cleanly, repeatedly, no leak
   or crash.
4. Subscribe to one hardcoded HA entity's state at runtime and update a label (validates §8).
5. Call one hardcoded HA service on tap (e.g. `light.toggle`).

If all five pass, the architecture is sound and you can build the registry, schema, storage,
and GUI on top with confidence. Read espcontrol's `components/espcontrol` first — it has
already crossed this exact bridge.

---

## 14. Repository layout

```
/
├─ components/
│  └─ espdash/                 # the external component (engine)
│     ├─ __init__.py           # ESPHome codegen / config schema for the component
│     ├─ espdash.h / .cpp      # component entry, screen zones, lifecycle
│     ├─ registry.h            # widget registry + ESPDASH_REGISTER_WIDGET macro
│     ├─ widget.h              # Widget contract, EntityState, ConfigSchema
│     ├─ binding.h / .cpp      # BindingService (runtime HA subscribe / call_service)
│     ├─ storage.h / .cpp      # LittleFS config load/save + migration
│     ├─ web/                  # device-side HTTP handlers + embedded gzipped SPA
│     ├─ layout.h / .cpp       # responsive grid layout
│     └─ widgets/
│        ├─ light.cpp
│        ├─ switch.cpp
│        ├─ sensor.cpp
│        ├─ climate.cpp
│        └─ system/            # clock, wifi_signal, badges (system widgets)
├─ web/                        # SPA source (built → embedded into components/espdash/web)
├─ boards/                     # per-board base YAML + ESP Web Tools manifests
│  ├─ guition-jc8048w550.yaml
│  ├─ guition-jc827w543.yaml
│  └─ wt32-sc01-plus.yaml
├─ builds/                     # CI-produced precompiled firmware (or via Releases)
├─ docs/                       # GitHub Pages: install guide, web-flash, board list
├─ .github/workflows/          # build matrix per board, publish images + manifests
├─ DESIGN.md                   # this file
├─ LICENSE                     # MIT
└─ README.md
```

The beginner-facing snippet in README is essentially:

```yaml
external_components:
  - source: github://<you>/espdash@main
    components: [espdash]

# + the board's base config from boards/ (display, touch, lvgl stub, wifi/api/ota,
#   ap fallback, captive_portal, improv_serial)

espdash:
```

---

## 15. Open questions / decisions to revisit
- **Diffing vs teardown-rebuild** on config change — start with teardown-rebuild; revisit
  only if rebuild flicker/latency is a problem.
- **Token handling** for the browser entity picker — confirm CORS works calling HA from the
  GUI origin; document the exact HA setup. Decide whether to ever persist the token on device.
- **Exact ESPHome API internals** for runtime state subscription / service calls (§8) — pin
  an ESPHome version and verify the symbol names against that version's source.
- **MDI font subset** size vs flash budget — bundle a curated subset, not the whole set.
- **Multiple pages** navigation UX (swipe + indicator) — schema supports it from v1; rendering
  can land slightly later.
- **Per-board quirks** (rotation, color order, touch calibration) — keep these entirely in the
  `boards/` YAML, never in the engine.

---

## 16. Prior art (for orientation; we are intentionally different)
- **espcontrol** (jtenniswood) — closest reference: ESPHome + C++ engine + on-device web
  config, but single-board and button-only. We generalize it: many entity-type widgets,
  multiple boards, native-C++ throughout. Read its component source for the LVGL-runtime and
  HA-binding patterns.
- **openHASP** — LVGL firmware with runtime JSONL config over MQTT. We differ: ESPHome-native
  (single update mechanism), native HA API instead of MQTT, responsive grid instead of
  absolute coordinates, schema-driven web GUI instead of hand-written JSONL.
- **esphome-modular-lvgl-buttons** (agillis) — good tile/detail-page pattern worth borrowing.

---

## 17. Guiding constraints (keep these true)
1. No dashboard config in YAML — only the ESPHome device file is YAML.
2. The engine never references display/touch directly — it rides ESPHome's `lvgl:`.
3. PSRAM boards only; don't compromise for low-memory parts.
4. Adding a widget type = one file + one registration, no GUI changes.
5. Config lives on the device; `schema_version` from day one.
6. Single LVGL instance, main-loop only — never touch LVGL off the main loop.
