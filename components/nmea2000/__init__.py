import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_ID
from esphome.core import TimePeriod

CODEOWNERS = ["@magliaral"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["sensor"]

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

CONF_TRANSMIT = "transmit"
CONF_PGN = "pgn"
CONF_INTERVAL = "interval"
CONF_INSTANCE = "instance"
CONF_VOLTAGE = "voltage"
CONF_CURRENT = "current"
CONF_TEMPERATURE = "temperature"
CONF_DC_TYPE = "dc_type"
CONF_STATE_OF_CHARGE = "state_of_charge"
CONF_TIME_REMAINING = "time_remaining"
CONF_SOURCE = "source"
CONF_ACTUAL = "actual"

PGN_BATTERY_STATUS = "battery_status"
PGN_DC_DETAILED_STATUS = "dc_detailed_status"
PGN_TEMPERATURE = "temperature"
PGN_TEMPERATURE_EXTENDED = "temperature_extended"
PGN_HUMIDITY = "humidity"

nmea2000_ns = cg.esphome_ns.namespace("nmea2000")
Nmea2000Component = nmea2000_ns.class_("Nmea2000Component", cg.Component)

DcType = nmea2000_ns.enum("DcType", is_class=True)
DC_TYPES = {
    "battery": DcType.BATTERY,
    "alternator": DcType.ALTERNATOR,
    "converter": DcType.CONVERTER,
    "solar_cell": DcType.SOLAR_CELL,
    "wind_generator": DcType.WIND_GENERATOR,
}

TempSource = nmea2000_ns.enum("TempSource", is_class=True)
TEMP_SOURCES = {
    "sea": TempSource.SEA,
    "outside": TempSource.OUTSIDE,
    "inside": TempSource.INSIDE,
    "engine_room": TempSource.ENGINE_ROOM,
    "main_cabin": TempSource.MAIN_CABIN,
    "live_well": TempSource.LIVE_WELL,
    "bait_well": TempSource.BAIT_WELL,
    "refrigeration": TempSource.REFRIGERATION,
    "heating_system": TempSource.HEATING_SYSTEM,
    "dew_point": TempSource.DEW_POINT,
    "apparent_wind_chill": TempSource.APPARENT_WIND_CHILL,
    "theoretical_wind_chill": TempSource.THEORETICAL_WIND_CHILL,
    "heat_index": TempSource.HEAT_INDEX,
    "freezer": TempSource.FREEZER,
    "exhaust_gas": TempSource.EXHAUST_GAS,
    "shaft_seal": TempSource.SHAFT_SEAL,
}

HumiditySource = nmea2000_ns.enum("HumiditySource", is_class=True)
HUMIDITY_SOURCES = {
    "inside": HumiditySource.INSIDE,
    "outside": HumiditySource.OUTSIDE,
}

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


def _transmit_base(default_interval):
    return cv.Schema(
        {
            cv.Optional(CONF_INTERVAL, default=default_interval): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=TimePeriod(milliseconds=100)),
            ),
            cv.Optional(CONF_INSTANCE, default=0): cv.int_range(min=0, max=252),
        }
    )


TRANSMIT_SCHEMA = cv.typed_schema(
    {
        PGN_BATTERY_STATUS: _transmit_base("2s").extend(
            {
                cv.Required(CONF_VOLTAGE): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_CURRENT): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
            }
        ),
        PGN_DC_DETAILED_STATUS: cv.All(
            _transmit_base("2s").extend(
                {
                    cv.Optional(CONF_DC_TYPE, default="battery"): cv.enum(
                        DC_TYPES, lower=True
                    ),
                    cv.Optional(CONF_STATE_OF_CHARGE): cv.use_id(sensor.Sensor),
                    cv.Optional(CONF_TIME_REMAINING): cv.use_id(sensor.Sensor),
                }
            ),
            cv.has_at_least_one_key(CONF_STATE_OF_CHARGE, CONF_TIME_REMAINING),
        ),
        PGN_TEMPERATURE: _transmit_base("5s").extend(
            {
                cv.Required(CONF_SOURCE): cv.enum(TEMP_SOURCES, lower=True),
                cv.Required(CONF_ACTUAL): cv.use_id(sensor.Sensor),
            }
        ),
        PGN_TEMPERATURE_EXTENDED: _transmit_base("5s").extend(
            {
                cv.Required(CONF_SOURCE): cv.enum(TEMP_SOURCES, lower=True),
                cv.Required(CONF_ACTUAL): cv.use_id(sensor.Sensor),
            }
        ),
        PGN_HUMIDITY: _transmit_base("5s").extend(
            {
                cv.Optional(CONF_SOURCE, default="inside"): cv.enum(
                    HUMIDITY_SOURCES, lower=True
                ),
                cv.Required(CONF_ACTUAL): cv.use_id(sensor.Sensor),
            }
        ),
    },
    key=CONF_PGN,
    lower=True,
)


def _validate_unique_instances(config):
    seen = set()
    for item in config.get(CONF_TRANSMIT, []):
        key = (item[CONF_PGN], item[CONF_INSTANCE])
        if key in seen:
            raise cv.Invalid(
                f"Duplicate transmit entry: pgn '{item[CONF_PGN]}' with instance "
                f"{item[CONF_INSTANCE]} is configured more than once. Use a different "
                f"'instance' for each entry of the same pgn."
            )
        seen.add(key)
    return config


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
            cv.Optional(CONF_TRANSMIT): cv.ensure_list(TRANSMIT_SCHEMA),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_unique_instances,
    cv.only_on_esp32,
)


async def _optional_sensor(item, key):
    if key not in item:
        return cg.nullptr
    return await cg.get_variable(item[key])


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

    for item in config.get(CONF_TRANSMIT, []):
        pgn = item[CONF_PGN]
        interval = item[CONF_INTERVAL].total_milliseconds
        instance = item[CONF_INSTANCE]
        if pgn == PGN_BATTERY_STATUS:
            voltage = await cg.get_variable(item[CONF_VOLTAGE])
            current = await _optional_sensor(item, CONF_CURRENT)
            temperature = await _optional_sensor(item, CONF_TEMPERATURE)
            cg.add(var.add_battery_status(interval, instance, voltage, current, temperature))
        elif pgn == PGN_DC_DETAILED_STATUS:
            soc = await _optional_sensor(item, CONF_STATE_OF_CHARGE)
            time_remaining = await _optional_sensor(item, CONF_TIME_REMAINING)
            cg.add(
                var.add_dc_detailed_status(
                    interval, instance, item[CONF_DC_TYPE], soc, time_remaining
                )
            )
        elif pgn == PGN_TEMPERATURE:
            actual = await cg.get_variable(item[CONF_ACTUAL])
            cg.add(var.add_temperature(interval, instance, item[CONF_SOURCE], actual))
        elif pgn == PGN_TEMPERATURE_EXTENDED:
            actual = await cg.get_variable(item[CONF_ACTUAL])
            cg.add(
                var.add_temperature_extended(interval, instance, item[CONF_SOURCE], actual)
            )
        elif pgn == PGN_HUMIDITY:
            actual = await cg.get_variable(item[CONF_ACTUAL])
            cg.add(var.add_humidity(interval, instance, item[CONF_SOURCE], actual))
