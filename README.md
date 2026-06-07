# PixGate

Turn an LVGL-capable, touch-display ESP32 board into a Home Assistant dashboard — configured
entirely from your browser, with no dashboard layout in YAML.

PixGate is an ESPHome external component that rides on ESPHome's `lvgl:` component and builds a
widget dashboard at runtime. It subscribes to Home Assistant entities and calls HA services over
the native API, so the panel can both show state and control devices. The whole layout (zones,
grid, widgets, theme) is edited live from a web GUI served by the device — the only YAML you write
is the standard board definition plus the `pixgate:` line.

## How it works

- **Rides on `lvgl:`** — PixGate never touches the display or touch drivers directly. It builds its
  widget tree on whatever LVGL display the `lvgl:` component sets up, so any board ESPHome's LVGL
  stack supports will work.
- **Runtime configuration** — the dashboard is a JSON document stored on the device flash
  (default `/pixgate.json`). Edit it from the web GUI; nothing about the layout lives in YAML.
- **Web GUI** — the device serves a shell page at `/` that loads the Svelte configuration app, and
  exposes `/api/*` endpoints for the config, widget registry, device info, and icons. The app
  bundle is loaded from GitHub Pages by default (override with `spa_base_url`).
- **Home Assistant binding** — entity subscriptions and service calls go over the ESPHome native
  API at runtime, so entities are chosen in the GUI rather than baked in at build time.

## Configuration

The only PixGate-specific YAML is the `pixgate:` block on top of a normal display + touch + `lvgl:`
device definition. See [sample.yaml](sample.yaml) for a full template, and the [devices/](devices/)
folder for ready-made per-board base configs.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/jonnybergdahl/PixGate
    components: [pixgate]

# Standard board hardware — replace for your panel (see devices/).
display:
  - platform: ...
    id: main_display

touchscreen:
  - platform: ...
    id: main_touch

# Minimal LVGL stub; PixGate builds its widget tree on top of this.
lvgl:

# The engine. No dashboard layout here by design.
pixgate:
```

Enable "Allow the device to perform Home Assistant actions" in the ESPHome integration so widget
service calls (toggling lights, setting climate, etc.) work.

### Options

| Option         | Default                                   | Description                                                     |
| -------------- | ----------------------------------------- | --------------------------------------------------------------- |
| `config_path`  | `/pixgate.json`                           | On-device path of the dashboard JSON document.                  |
| `spa_base_url` | `https://jonnybergdahl.github.io/PixGate` | Base URL the device shell page loads the web GUI bundle from.   |

## Supported boards

Per-device base configs live in [devices/](devices/):

- **Guition JC8048W550** — ESP32-S3, 7" 800×480 RGB panel, GT911 touch.
- **Guition JC827W543** — ESP32-S3, 4.3" 480×272 QSPI panel (`mipi_spi`), GT911 touch.
- **Sunton ESP32-8048S070C** — ESP32-S3, 7" 800×480 RGB panel, GT911 touch.
- **Waveshare ESP32-P4 86 Panel** — ESP32-P4 + hosted ESP32-C6 WiFi, 4" 720×720 MIPI-DSI, GT911 touch.
- **WT32-SC01 Plus** — ESP32-S3, 3.5" 480×320 ST7796 panel, FT63x6 touch.

## Widgets

- **System** (header zone): clock (reads HA `sensor.time`), WiFi signal.
- **Entity**: light, switch, sensor / binary_sensor, climate.

Adding a widget type is a single C++ file in `components/pixgate/` — the web GUI discovers it from
the device's widget registry, so no GUI changes are needed.

## Theme

Light and dark themes are built in and selected from the GUI's display settings. Widgets read the
active palette so the whole dashboard stays consistent.

## Building

`./build.sh` compiles every config in `devices/`; pass a name to build just one:

```bash
./build.sh                       # build every devices/*.yaml
./build.sh guition-jc8048w550    # build a single board
```

Requires `esphome` on your PATH (`pip install esphome`). Firmware images are copied into
`firmware/<device>/`.

See [DESIGN.md](DESIGN.md) for the architecture and design constraints.
