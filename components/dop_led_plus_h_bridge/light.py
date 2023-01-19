import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output, remote_transmitter
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
CONF_OUTPUT_2V5_ID = "output_2v5_id"
CONF_OUTPUT_P2_ID = "output_p2_id"
CONF_OUTPUT_N1_PWM_ID = "output_n1_pwm_id"
CONF_OUTPUT_N2_ID = "output_n2_id"

dop_led_plus_h_bridge_ns = cg.esphome_ns.namespace("dop_led_plus_h_bridge")
DoPLEDOutput = dop_led_plus_h_bridge_ns.class_("DoPLEDOutput", light.AddressableLight)

RGB_ORDER = dop_led_plus_h_bridge_ns.enum("EOrder")

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
        cv.Required(CONF_OUTPUT_2V5_ID): cv.use_id(output.BinaryOutput),
        cv.Required(CONF_OUTPUT_P2_ID): cv.use_id(output.BinaryOutput),
        cv.Required(CONF_OUTPUT_N1_PWM_ID): cv.use_id(output.FloatOutput),
        cv.Required(CONF_OUTPUT_N2_ID): cv.use_id(output.BinaryOutput),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    output_2v5 = await cg.get_variable(config[CONF_OUTPUT_2V5_ID])
    output_p2 = await cg.get_variable(config[CONF_OUTPUT_P2_ID])
    output_n1_pwm = await cg.get_variable(config[CONF_OUTPUT_N1_PWM_ID])
    output_n2 = await cg.get_variable(config[CONF_OUTPUT_N2_ID])
    xmitr_var = await cg.get_variable(config[CONF_TRANSMITTER_ID])

    var = cg.new_Pvariable(
        config[CONF_OUTPUT_ID],
        xmitr_var,
        output_p2,
        output_n1_pwm,
        output_n2,
        output_2v5,
    )
    await cg.register_component(var, config)
    await light.register_light(var, config)

    if CONF_MAX_REFRESH_RATE in config:
        cg.add(var.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))

    if CONF_RGB_ORDER in config:
        cg.add(var.set_rgb_order(config[CONF_RGB_ORDER]))

    cg.add(var.set_num_header_bits(config[CONF_NUM_HEADER_BITS]))
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
