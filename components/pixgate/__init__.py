import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server, display, lvgl, api
from esphome.const import CONF_ID, CONF_WEB_SERVER_ID, CONF_DISPLAY_ID

AUTO_LOAD = ["web_server", "display", "network", "json", "lvgl", "api", "littlefs"]

pixgate_ns = cg.esphome_ns.namespace("pixgate")
PixGate = pixgate_ns.class_("PixGate", cg.Component)

# Add dependencies
cg.add_library("bblanchon/ArduinoJson", "6.21.3")

CONF_ROWS = "rows"
CONF_COLUMNS = "columns"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(PixGate),
    cv.GenerateID(CONF_WEB_SERVER_ID): cv.use_id(web_server.WebServer),
    cv.Required(CONF_DISPLAY_ID): cv.use_id(display.DisplayBuffer),
    cv.Optional(CONF_ROWS, default=2): cv.int_range(min=1, max=10),
    cv.Optional(CONF_COLUMNS, default=2): cv.int_range(min=1, max=10),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_rows(config[CONF_ROWS]))
    cg.add(var.set_columns(config[CONF_COLUMNS]))

    server = await cg.get_variable(config[CONF_WEB_SERVER_ID])
    cg.add(var.set_web_server(server))

    dis = await cg.get_variable(config[CONF_DISPLAY_ID])
    cg.add(var.set_display(dis))
