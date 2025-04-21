import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

# Import from the renamed component's __init__
from . import CONF_A7672_ID, A7672Component

# Depend on the renamed component
DEPENDENCIES = ["a7672"]

CONF_RSSI = "rssi"

CONFIG_SCHEMA = {
    # Use the renamed component ID
    cv.GenerateID(CONF_A7672_ID): cv.use_id(A7672Component),
    # Schema for the 'rssi' sensor (remains the same, using dBm)
    cv.Optional(CONF_RSSI): sensor.sensor_schema(
        unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    # Get the variable for the renamed component
    a7672_component = await cg.get_variable(config[CONF_A7672_ID])

    if CONF_RSSI in config:
        sens = await sensor.new_sensor(config[CONF_RSSI])
        # Call the setter method on the renamed component
        cg.add(a7672_component.set_rssi_sensor(sens))