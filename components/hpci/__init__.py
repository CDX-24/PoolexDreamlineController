import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

MULTI_CONF = True
AUTO_LOAD = ["hpci"]

hpci_ns = cg.esphome_ns.namespace("hpci")
HeatPumpControlInterface = hpci_ns.class_("HeatPumpController", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HeatPumpControlInterface),
        
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)