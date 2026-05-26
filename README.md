# PixGate
ESPHome support for Display devices

PixGate is an ESPHome component that allows display-equipped development boards to connect to Home Assistant via WebSockets and display/control devices using widgets.

## Configuration

Add the following to your ESPHome YAML (see [sample.yaml](sample.yaml) for a full template):

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/jonnybergdahl/PixGate

pixgate:
  display_id: my_display
  web_server_id: my_web_server

display:
  - platform: ...
    id: my_display

web_server:
  id: my_web_server

api:
  homeassistant_states: true
```

## Features

- **LVGL Integration**: Uses LVGL for high-quality widget rendering.
- **Native API Integration**: Switched to the ESPHome Native API for better performance and easier configuration.
- **Grid Layout**: Configurable number of rows and columns for widget placement.
- **Widget System**: Extensible widget system for displaying information (currently supports LVGL-based text widgets).
- **Web Configuration**: Embedded web page at `/pixgate` for real-time widget management and persistence.
