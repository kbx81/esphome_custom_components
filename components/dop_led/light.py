import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, remote_transmitter
from esphome.components.remote_base import CONF_TRANSMITTER_ID
from esphome.const import (
    CONF_MAX_REFRESH_RATE,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_RGB_ORDER,
)

AUTO_LOAD = ["remote_transmitter"]

CODEOWNERS = ["@kbx81"]

CONF_NUM_HEADER_BITS = "num_header_bits"

dop_led_ns = cg.esphome_ns.namespace("dop_led")
DoPLEDOutput = dop_led_ns.class_("DoPLEDOutput", light.AddressableLight)

RGB_ORDER = dop_led_ns.enum("EOrder")

RGB_ORDER_OPTIONS = {
    "RGB": RGB_ORDER.RGB,
    "RBG": RGB_ORDER.RBG,
    "GRB": RGB_ORDER.GRB,
    "GBR": RGB_ORDER.GBR,
    "BRG": RGB_ORDER.BRG,
    "BGR": RGB_ORDER.BGR,
}

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(DoPLEDOutput),
        cv.Required(CONF_TRANSMITTER_ID): cv.use_id(
            remote_transmitter.RemoteTransmitterComponent
        ),
        cv.Required(CONF_NUM_HEADER_BITS): cv.positive_not_null_int,
        cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
        cv.Optional(CONF_RGB_ORDER): cv.enum(RGB_ORDER_OPTIONS),
        cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    xmitr_var = await cg.get_variable(config[CONF_TRANSMITTER_ID])

    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], xmitr_var)
    await cg.register_component(var, config)
    await light.register_light(var, config)

    if CONF_MAX_REFRESH_RATE in config:
        cg.add(var.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))

    # rgb_order = None
    if CONF_RGB_ORDER in config:
        # rgb_order = cg.RawExpression(config[CONF_RGB_ORDER])
        cg.add(var.set_rgb_order(config[CONF_RGB_ORDER]))

    cg.add(var.set_num_header_bits(config[CONF_NUM_HEADER_BITS]))
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
