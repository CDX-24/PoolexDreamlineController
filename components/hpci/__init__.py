import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_PIN,
)

MULTI_CONF = False

hpci_ns = cg.esphome_ns.namespace("hpci")
settings_ns = cg.esphome_ns.namespace("settings")
HeatPumpControlInterface = hpci_ns.class_("HeatPumpController", cg.Component)

CONF_DEFROST_AUTO_ENABLE_TIME = "defrost_auto_enable_time"
CONF_DEFROST_ENABLE_TEMP = "defrost_enable_temp"
CONF_DEFROST_DISABLE_TEMP = "defrost_disable_temp"
CONF_DEFROST_MAX_DURATION = "defrost_max_duration"
CONF_RESTART_OFFSET_TEMP = "restart_offset_temp"
CONF_COMPRESSOR_STOP_MARGIN_TEMP = "compressor_stop_margin_temp"
CONF_THERMAL_PROTECTION = "thermal_protection"
CONF_MAX_TEMP = "maximum_temp"
CONF_STOP_WHEN_REACHED_DELAY = "stop_when_temp_reached_delay"
CONF_SPECIAL_CTRL_MODE = "special_control_mode"
CONF_ACTION = "action"
CONF_OP_MODE = "operation_mode"
CONF_AUTO_RESTART = "auto_restart"

actionEnum = settings_ns.enum("actionEnum")
HP_ACTION = {
    "HEAT": actionEnum.HEAT,
    "COOL": actionEnum.COOL,
}
modeEnum = settings_ns.enum("modeEnum")
HP_OP_MODE = {
    "HEAT_ONLY": modeEnum.HEAT_ONLY,
    "COLD_ONLY": modeEnum.COLD_ONLY,
    "BOTH": modeEnum.BOTH,
}
errorEnum = settings_ns.enum("hpErrorEnum")
HP_ERROR = {
    "NONE": errorEnum.NONE,
    "PD": errorEnum.PD,
}

AUTO_LOAD = ["hpci"]


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HeatPumpControlInterface),
            cv.Required(CONF_PIN): cv.positive_int,  # Add this line for the GPIO pin
            cv.Optional(CONF_DEFROST_AUTO_ENABLE_TIME, default=40): cv.int_range(
                min=30, max=90
            ),
            cv.Optional(CONF_DEFROST_ENABLE_TEMP, default=7): cv.int_range(min=1),
            cv.Optional(CONF_DEFROST_DISABLE_TEMP, default=13): cv.int_range(
                min=2, max=30
            ),
            cv.Optional(CONF_DEFROST_MAX_DURATION, default=8): cv.int_range(
                min=0, max=15
            ),
            cv.Optional(CONF_RESTART_OFFSET_TEMP, default=2): cv.int_range(
                min=2, max=10
            ),
            cv.Optional(CONF_COMPRESSOR_STOP_MARGIN_TEMP, default=0): cv.int_range(
                min=0, max=3
            ),
            cv.Optional(CONF_THERMAL_PROTECTION, default=118): cv.int_range(
                min=90, max=120
            ),
            cv.Optional(CONF_MAX_TEMP, default=40): cv.int_range(min=40, max=65),
            cv.Optional(CONF_STOP_WHEN_REACHED_DELAY, default=15): cv.int_range(
                min=3, max=20
            ),
            cv.Optional(CONF_SPECIAL_CTRL_MODE, default=1): cv.int_range(min=0, max=1),
            cv.Optional(CONF_ACTION, default="HEAT"): cv.enum(HP_ACTION, upper=True),
            cv.Optional(CONF_OP_MODE, default="HEAT_ONLY"): cv.enum(
                HP_OP_MODE, upper=True
            ),
            cv.Optional(CONF_AUTO_RESTART, default=1): cv.int_range(min=0, max=1),
            cv.Optional(CONF_AUTO_RESTART, default=1): cv.int_range(min=0, max=1),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_pin(config[CONF_PIN]))  # Pass the pin to the C++ code

