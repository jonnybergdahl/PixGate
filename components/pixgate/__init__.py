"""PixGate — ESPHome external component (engine).

Builds a runtime LVGL dashboard on top of ESPHome's ``lvgl:`` component. The whole dashboard
is configured at runtime from the web GUI; the only YAML is this component plus the standard
device definition (display + touch + lvgl stub + wifi/api/web_server). See DESIGN.md.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.const import CONF_ID

CODEOWNERS = ["@jonnybergdahl"]

# We ride on ESPHome's lvgl component for all hardware glue (DESIGN.md §4) and auto-load the
# shared web server + json helpers used by the device-side API (§11).
DEPENDENCIES = ["lvgl"]
AUTO_LOAD = ["web_server_base", "json"]

CONF_CONFIG_PATH = "config_path"
CONF_WEB_ID = "web_id"
CONF_WEB_SERVER_BASE_ID = "web_server_base_id"
CONF_SPA_BASE_URL = "spa_base_url"

# Where the device shell page loads the Svelte GUI bundle from. The shell HTML is served by
# the device (so the SPA's /api/* calls stay same-origin); only the JS/CSS assets are pulled
# from GitHub Pages. Override per fork in YAML if you host the bundle elsewhere.
DEFAULT_SPA_BASE_URL = "https://jonnybergdahl.github.io/PixGate"

pixgate_ns = cg.esphome_ns.namespace("pixgate")
PixGate = pixgate_ns.class_("PixGate", cg.Component)
PixGateWeb = pixgate_ns.class_("PixGateWeb", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PixGate),
        cv.GenerateID(CONF_WEB_ID): cv.declare_id(PixGateWeb),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
        # On-device path of the versioned dashboard JSON document on LittleFS (DESIGN.md §9).
        cv.Optional(CONF_CONFIG_PATH, default="/pixgate.json"): cv.string,
        # Base URL the device shell page loads the Svelte GUI bundle from (no trailing slash).
        cv.Optional(CONF_SPA_BASE_URL, default=DEFAULT_SPA_BASE_URL): cv.url,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # Ensure the engine and web headers are included in the generated firmware.
    cg.add_global(cg.RawStatement('#include "esphome/components/pixgate/pixgate.h"'))
    cg.add_global(
        cg.RawStatement('#include "esphome/components/pixgate/web_server.h"')
    )

    # The runtime binding (binding.cpp) subscribes to arbitrary HA entity states and calls
    # arbitrary HA services through the native API. Those API features are compiled out by
    # default, so opt in explicitly the way the homeassistant platforms do.
    cg.add_define("USE_API_HOMEASSISTANT_STATES")
    cg.add_define("USE_API_HOMEASSISTANT_SERVICES")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_config_path(config[CONF_CONFIG_PATH]))

    # Wire the device-side web GUI to the shared web server and the engine.
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    web = cg.new_Pvariable(config[CONF_WEB_ID])
    await cg.register_component(web, config)
    cg.add(web.set_base(paren))
    cg.add(web.set_pixgate(var))
    cg.add(web.set_spa_base_url(config[CONF_SPA_BASE_URL]))
