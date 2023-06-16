import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_INDEX
)
from . import HeatPumpControlInterface, hpci_ns

#DallasTemperatureSensor = hpci_ns.class_("HeatPumpControlInterface", sensor.Sensor)

CONFIG_SCHEMA = cv.All(
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_DALLAS_ID])
    var = await sensor.new_sensor(config)