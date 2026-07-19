import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_CHANNEL,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)
from . import Rf6036bComponent, rf6036b_ns

CONF_RF6036B_ID = "rf6036b_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RF6036B_ID): cv.use_id(Rf6036bComponent),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=3),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_RF6036B_ID])
    channel = config[CONF_CHANNEL]

    temp_var = None
    hum_var = None

    if CONF_TEMPERATURE in config:
        temp_var = await sensor.new_sensor(config[CONF_TEMPERATURE])

    if CONF_HUMIDITY in config:
        hum_var = await sensor.new_sensor(config[CONF_HUMIDITY])

    cg.add(
        parent.add_channel(
            channel,
            temp_var if temp_var is not None else cg.nullptr,
            hum_var if hum_var is not None else cg.nullptr,
        )
    )
