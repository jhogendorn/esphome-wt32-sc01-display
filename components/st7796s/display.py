import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi
from esphome.components import display
from esphome.const import (
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_RESET_PIN,
    CONF_BACKLIGHT_PIN,
    CONF_PAGES,
)
from . import st7796s_ns

CODEOWNERS = ["@Ryan6338"]

DEPENDENCIES = ["spi"]

AUTO_LOAD = ["psram"]

CONF_DEVICE_WIDTH = "device_width"
CONF_DEVICE_HEIGHT = "device_height"
CONF_ROW_START = "row_start"
CONF_COL_START = "col_start"
CONF_EIGHT_BIT_COLOR = "eight_bit_color"
CONF_USE_BGR = "use_bgr"
CONF_INVERT_COLORS = "invert_colors"

SPIST7796S = st7796s_ns.class_(
    "ST7796S", cg.PollingComponent, display.DisplayBuffer, spi.SPIDevice
)


ST7796S_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("1s"))

CONFIG_SCHEMA = cv.All(
    ST7796S_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SPIST7796S),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DEVICE_WIDTH): cv.int_,
            cv.Required(CONF_DEVICE_HEIGHT): cv.int_,
            cv.Required(CONF_COL_START): cv.int_,
            cv.Required(CONF_ROW_START): cv.int_,
            cv.Optional(CONF_BACKLIGHT_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_EIGHT_BIT_COLOR, default=False): cv.boolean,
            cv.Optional(CONF_USE_BGR, default=False): cv.boolean,
            cv.Optional(CONF_INVERT_COLORS, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema()),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def setup_st7796s(var, config):
    await cg.register_component(var, config)
    await display.register_display(var, config)

    if CONF_RESET_PIN in config:
        reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    if CONF_BACKLIGHT_PIN in config:
        bl = await cg.gpio_pin_expression(config[CONF_BACKLIGHT_PIN])
        cg.add(var.set_backlight_pin(bl))


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_DEVICE_WIDTH],
        config[CONF_DEVICE_HEIGHT],
        config[CONF_COL_START],
        config[CONF_ROW_START],
        config[CONF_EIGHT_BIT_COLOR],
        config[CONF_USE_BGR],
        config[CONF_INVERT_COLORS],
    )
    await setup_st7796s(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
