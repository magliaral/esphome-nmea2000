import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_ICON,
    CONF_STATE_CLASS,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SPEED,
    DEVICE_CLASS_WIND_SPEED,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREES,
    UNIT_METER,
)

from . import CONF_NMEA2000_ID, Nmea2000Component, nmea2000_ns

DEPENDENCIES = ["nmea2000"]

CONF_WIND_REFERENCE = "wind_reference"

UNIT_KNOT = "kn"

Nmea2000Sensor = nmea2000_ns.class_("Nmea2000Sensor", sensor.Sensor)
SensorType = nmea2000_ns.enum("SensorType", is_class=True)
WindReference = nmea2000_ns.enum("WindReference", is_class=True)

WIND_REFERENCES = {
    "apparent": WindReference.APPARENT,
    "true_boat": WindReference.TRUE_BOAT,
    "true_water": WindReference.TRUE_WATER,
    "true_north": WindReference.TRUE_NORTH,
    "magnetic": WindReference.MAGNETIC,
}

WIND_TYPES = ("WIND_SPEED", "WIND_ANGLE")

SENSOR_TYPES = {
    "LATITUDE": {
        CONF_TYPE: SensorType.LATITUDE,
        CONF_UNIT_OF_MEASUREMENT: UNIT_DEGREES,
        CONF_ACCURACY_DECIMALS: 6,
        CONF_ICON: "mdi:latitude",
    },
    "LONGITUDE": {
        CONF_TYPE: SensorType.LONGITUDE,
        CONF_UNIT_OF_MEASUREMENT: UNIT_DEGREES,
        CONF_ACCURACY_DECIMALS: 6,
        CONF_ICON: "mdi:longitude",
    },
    "SPEED_OVER_GROUND": {
        CONF_TYPE: SensorType.SPEED_OVER_GROUND,
        CONF_UNIT_OF_MEASUREMENT: UNIT_KNOT,
        CONF_ACCURACY_DECIMALS: 1,
        CONF_ICON: "mdi:speedometer",
        CONF_DEVICE_CLASS: DEVICE_CLASS_SPEED,
    },
    "COURSE_OVER_GROUND": {
        CONF_TYPE: SensorType.COURSE_OVER_GROUND,
        CONF_UNIT_OF_MEASUREMENT: UNIT_DEGREES,
        CONF_ACCURACY_DECIMALS: 0,
        CONF_ICON: "mdi:compass-outline",
    },
    "HEADING": {
        CONF_TYPE: SensorType.HEADING,
        CONF_UNIT_OF_MEASUREMENT: UNIT_DEGREES,
        CONF_ACCURACY_DECIMALS: 0,
        CONF_ICON: "mdi:compass",
    },
    "WATER_DEPTH": {
        CONF_TYPE: SensorType.WATER_DEPTH,
        CONF_UNIT_OF_MEASUREMENT: UNIT_METER,
        CONF_ACCURACY_DECIMALS: 1,
        CONF_ICON: "mdi:waves-arrow-down",
        CONF_DEVICE_CLASS: DEVICE_CLASS_DISTANCE,
    },
    "SPEED_THROUGH_WATER": {
        CONF_TYPE: SensorType.SPEED_THROUGH_WATER,
        CONF_UNIT_OF_MEASUREMENT: UNIT_KNOT,
        CONF_ACCURACY_DECIMALS: 1,
        CONF_ICON: "mdi:speedometer",
        CONF_DEVICE_CLASS: DEVICE_CLASS_SPEED,
    },
    "WIND_SPEED": {
        CONF_TYPE: SensorType.WIND_SPEED,
        CONF_UNIT_OF_MEASUREMENT: UNIT_KNOT,
        CONF_ACCURACY_DECIMALS: 1,
        CONF_ICON: "mdi:weather-windy",
        CONF_DEVICE_CLASS: DEVICE_CLASS_WIND_SPEED,
    },
    "WIND_ANGLE": {
        CONF_TYPE: SensorType.WIND_ANGLE,
        CONF_UNIT_OF_MEASUREMENT: UNIT_DEGREES,
        CONF_ACCURACY_DECIMALS: 0,
        CONF_ICON: "mdi:windsock",
    },
}


def _validate_wind_reference(config):
    if CONF_WIND_REFERENCE in config and config[CONF_TYPE] not in WIND_TYPES:
        raise cv.Invalid(
            "'wind_reference' is only valid for WIND_SPEED and WIND_ANGLE sensors"
        )
    return config


def _set_defaults_based_on_type(config):
    defaults = SENSOR_TYPES[config[CONF_TYPE]]
    if CONF_STATE_CLASS not in config:
        config[CONF_STATE_CLASS] = sensor.validate_state_class(STATE_CLASS_MEASUREMENT)
    for key in (
        CONF_UNIT_OF_MEASUREMENT,
        CONF_ICON,
        CONF_ACCURACY_DECIMALS,
        CONF_DEVICE_CLASS,
    ):
        if key not in config and key in defaults:
            config[key] = defaults[key]
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(Nmea2000Sensor).extend(
        {
            cv.GenerateID(CONF_NMEA2000_ID): cv.use_id(Nmea2000Component),
            cv.Required(CONF_TYPE): cv.enum(SENSOR_TYPES, upper=True),
            cv.Optional(CONF_WIND_REFERENCE): cv.enum(WIND_REFERENCES, lower=True),
        }
    ),
    _validate_wind_reference,
)

FINAL_VALIDATE_SCHEMA = _set_defaults_based_on_type


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NMEA2000_ID])
    var = await sensor.new_sensor(config)
    cg.add(var.set_type(SENSOR_TYPES[config[CONF_TYPE]][CONF_TYPE]))
    if config[CONF_TYPE] in WIND_TYPES:
        wind_reference = config.get(CONF_WIND_REFERENCE, "apparent")
        cg.add(var.set_wind_reference(WIND_REFERENCES[wind_reference]))
    cg.add(hub.register_sensor(var))
