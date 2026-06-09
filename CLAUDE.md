# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What PixGate is

An **ESPHome external component** (C++ namespace `esphome::pixgate`) that turns a PSRAM-equipped,
LVGL-capable, touch-display ESP32 board into a Home Assistant dashboard. The defining principle:
**no dashboard layout lives in YAML** — the entire widget tree is configured at runtime from a web
GUI and stored as a JSON document on device flash. The only YAML is the standard board hardware
definition plus the one-line `pixgate:` block.

`DESIGN.md` is the source-of-truth architecture document and explains the *why* behind most
decisions. Read it before making structural changes; if you change a design decision, update
`DESIGN.md` and note why. (Note: `DESIGN.md` still uses the old working name `espdash`/`ESPDASH_*`
in places — the shipped component is `pixgate`/`PIXGATE_*`.)

## Hard constraints

- **ESP-IDF framework only** — never Arduino. All device YAMLs use `framework: type: esp-idf`.
- **Never touch the display or touch drivers.** The engine rides on ESPHome's `lvgl:` component and
  operates only on the raw `lv_obj_*` C API on whatever LVGL display `lvgl:` set up. The single
  exception is a pointer to `LvglComponent`, held solely to pause/resume rendering around OTA.
- **PSRAM boards only.** Do not compromise the design for low-memory parts.
- **Single LVGL instance, main-loop only.** ESPHome is single-threaded; never call `lv_*` from
  another task or ISR — there is no locking.
- **Config lives on the device** as a versioned JSON document (`schema_version` from day one).

## Build / upload

```bash
./build.sh                       # compile every devices/*.yaml, copy images to firmware/<device>/
./build.sh wt32-sc01-plus        # build a single board (matches devices/<name>.yaml)
./upload.sh wt32-sc01-plus       # OTA upload (mDNS); append an IP to target explicitly
```

Requires `esphome` on PATH (`pip install esphome`). ESPHome creates its `.esphome/` working dir
inside `devices/`, not the repo root.

### Build verification gotchas

- **Do not pipe build output through `tail`/`head`** — `./build.sh | tail` hides compilation
  failures behind a success-looking tail. Read the full output (or check the exit status).
- **Adding a new `.cpp` to `components/pixgate/`** requires a clean `.pioenvs` for that board —
  ESPHome's incremental build will not pick up the new translation unit otherwise. Wipe the board's
  `.pioenvs` directory before rebuilding.

### Web GUI

```bash
cd web && npm ci && npm run dev      # local dev against a device's /api
cd web && npm run build              # emits web/dist/pixgate.js + pixgate.css
```

The GUI is a **Vite + Svelte 5 SPA** (`web/`). The device serves only a tiny shell page at `/`
that pulls `pixgate.js`/`pixgate.css` from GitHub Pages (`spa_base_url`, default
`https://jonnybergdahl.github.io/PixGate`); the shell being device-served keeps the SPA's `/api/*`
calls same-origin. The Vite config emits exactly those two fixed filenames (no hashed chunks) so
the shell can predict them. CI (`.github/workflows/pages.yml`) builds `web/` and deploys to Pages
on push to `main` touching `web/**`.

## Architecture

Three layers cooperate at **runtime** (nothing about the dashboard is known at build time):

1. **Engine** (`components/pixgate/pixgate.{h,cpp}`) — builds a single root screen of three vertical
   zones: **header**, optional **badge row**, and a **main window** (responsive grid, `layout.{h,cpp}`).
   On boot it loads the JSON config and instantiates the widget tree; on config change it does a
   **teardown + rebuild** of the affected zones (not diffing). Registers as an
   `ota::OTAGlobalStateListener` to pause LVGL during OTA.

2. **Widget system** — the extensibility core (DESIGN.md §7):
   - `widget.h` — the `Widget` contract (`type_id`, `supported_domains`, `build`, `on_state`,
     `on_event`, `schema`, `destroy`), plus `EntityState` and the self-describing `ConfigSchema`
     (`ConfigField` with types ENTITY/STRING/BOOL/INT/ENUM/COLOR/ICON).
   - `registry.h` — a Meyers-singleton `WidgetRegistry`; widget types self-register at static-init
     via `PIXGATE_REGISTER_WIDGET("light", LightWidget)`. The registry is exposed over the JSON API
     so the GUI enumerates types + schemas and stays in sync with the firmware automatically.
   - `widget_base.h` — shared helpers (`make_tile`, `style_tile`, `style_as_badge`, `parse_color`,
     the `widget_event_trampoline` event router, and a `TileWidget` base with default `destroy()`).
   - Widget implementations are one file each: entity widgets `light.cpp`, `switch.cpp`,
     `sensor.cpp`, `climate.cpp`; system widgets `clock.cpp`, `wifi_signal.cpp`.

   **Adding a widget type is one new `.cpp`** subclassing `Widget` (or `TileWidget`) + one
   `PIXGATE_REGISTER_WIDGET` line. No GUI changes — the GUI discovers it via the registry/schema
   API. Design new contract changes against `climate` (the most complex widget), not `switch`.

3. **Runtime HA binding** (`binding.{h,cpp}`) — `BindingService` wraps ESPHome's native API to
   `subscribe(entity_id, callback)` and `call_service(domain, service, data)` for *arbitrary*
   entities chosen at runtime (the compile-time `homeassistant:` platforms can't do this). It
   dedupes subscriptions and caches `entity_id -> EntityState` to seed freshly built widgets.
   `__init__.py` opts into the required API features via `USE_API_HOMEASSISTANT_STATES` /
   `USE_API_HOMEASSISTANT_SERVICES`. Users must enable "Allow the device to perform Home Assistant
   actions" in the ESPHome integration for service calls to work.

Supporting files: `storage.{h,cpp}` (LittleFS config load/save, atomic temp-file+rename, migration),
`theme.{h,cpp}` (built-in light/dark palettes; `active_theme()` is read by every widget),
`web_server.{h,cpp}` (device-side shell page + `/api/*` handlers).

### Device-side JSON API (`/api/*`, GET/POST only — ESP-IDF web server registers no PUT)

- `GET /api/config` / `POST /api/config` — read / replace the dashboard JSON (validate → persist
  atomically → rebuild). `apply_config_json` leaves the live dashboard untouched if invalid.
- `GET /api/registry` — widget types, schemas, supported domains.
- `GET /api/device` — board geometry + version. `GET /api/icons` — available icon names.

### Web GUI internals (`web/src/lib/`)

`api.js` (device `/api/*` client), `ha.js` (browser-side Home Assistant entity discovery — the
browser, not the device, fetches HA's entity list using a user-supplied URL+token; this is the only
part that reaches outside the device), `model.js` (config document model), `theme.js` (browser-only
editor theme, independent of the device dashboard theme).

## Repo layout

- `components/pixgate/` — the external component (engine, widgets, web handlers, `__init__.py` codegen).
- `devices/*.yaml` — per-board base configs (display + touch + lvgl stub + wifi/api/ota/provisioning
  + `pixgate:`). Per-board hardware quirks (rotation, color order, touch calibration) belong here,
  never in the engine.
- `web/` — Svelte SPA source (built → GitHub Pages).
- `firmware/<device>/` — build outputs (`firmware.bin`, `.ota.bin`, `.factory.bin`).
- `sample.yaml` — full template for a new board. `DESIGN.md` — architecture source of truth.

## Conventions

- C++ namespace is `esphome::pixgate`; macros are `PIXGATE_*`. Match the existing file style.
- Comments reference `DESIGN.md` sections (e.g. `§7.2`) — keep that linkage when adding code.