import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

# Import from the renamed component's __init__
from . import CONF_A7672_ID, A7672Component

# Depend on the renamed component
DEPENDENCIES = ["a7672"]

CONF_REGISTERED = "registered"

CONFIG_SCHEMA = {
    # Use the renamed component ID
    cv.GenerateID(CONF_A7672_ID): cv.use_id(A7672Component),
    # Schema for the 'registered' binary sensor (remains the same)
    cv.Optional(CONF_REGISTERED): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config):
    # Get the variable for the renamed component
    a7672_component = await cg.get_variable(config[CONF_A7672_ID])

    if CONF_REGISTERED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_REGISTERED])
        # Call the setter method on the renamed component
        cg.add(a7672_component.set_registered_binary_sensor(sens))