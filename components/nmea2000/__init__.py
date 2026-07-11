import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

CODEOWNERS = ["@magliaral"]
DEPENDENCIES = ["esp32"]

CONF_NMEA2000_ID = "nmea2000_id"

CONF_CAN = "can"
CONF_TX_PIN = "tx_pin"
CONF_RX_PIN = "rx_pin"

CONF_NODE = "node"
CONF_SOURCE_ADDRESS = "source_address"
CONF_UNIQUE_NUMBER = "unique_number"
CONF_MANUFACTURER_CODE = "manufacturer_code"
CONF_DEVICE_FUNCTION = "device_function"
CONF_DEVICE_CLASS = "device_class"
CONF_PRODUCT_NAME = "product_name"
CONF_PRODUCT_CODE = "product_code"
CONF_SOFTWARE_VERSION = "software_version"

nmea2000_ns = cg.esphome_ns.namespace("nmea2000")
Nmea2000Component = nmea2000_ns.class_("Nmea2000Component", cg.Component)

NODE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SOURCE_ADDRESS, default=15): cv.int_range(min=0, max=251),
        cv.Optional(CONF_UNIQUE_NUMBER, default=1): cv.int_range(min=1, max=0x1FFFFF),
        cv.Optional(CONF_MANUFACTURER_CODE, default=2046): cv.int_range(min=0, max=2046),
        cv.Optional(CONF_DEVICE_FUNCTION, default=130): cv.uint8_t,
        cv.Optional(CONF_DEVICE_CLASS, default=25): cv.int_range(min=0, max=127),
        cv.Optional(CONF_PRODUCT_NAME, default="ESPHome NMEA2000"): cv.All(
            cv.string, cv.Length(max=32)
        ),
        cv.Optional(CONF_PRODUCT_CODE, default=100): cv.int_range(min=0, max=65534),
        cv.Optional(CONF_SOFTWARE_VERSION, default="1.0.0"): cv.All(
            cv.string, cv.Length(max=32)
        ),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Nmea2000Component),
            cv.Required(CONF_CAN): cv.Schema(
                {
                    cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_number,
                    cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_number,
                }
            ),
            cv.Optional(CONF_NODE, default={}): NODE_SCHEMA,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def to_code(config):
    cg.add_library("ttlappalainen/NMEA2000-library", "4.24.1")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    can = config[CONF_CAN]
    cg.add(var.set_can_pins(can[CONF_TX_PIN], can[CONF_RX_PIN]))

    node = config[CONF_NODE]
    cg.add(var.set_source_address(node[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_unique_number(node[CONF_UNIQUE_NUMBER]))
    cg.add(var.set_manufacturer_code(node[CONF_MANUFACTURER_CODE]))
    cg.add(var.set_device_function(node[CONF_DEVICE_FUNCTION]))
    cg.add(var.set_device_class(node[CONF_DEVICE_CLASS]))
    cg.add(var.set_product_name(node[CONF_PRODUCT_NAME]))
    cg.add(var.set_product_code(node[CONF_PRODUCT_CODE]))
    cg.add(var.set_software_version(node[CONF_SOFTWARE_VERSION]))
