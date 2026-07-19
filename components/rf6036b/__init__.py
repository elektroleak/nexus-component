import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import remote_base
from esphome.const import CONF_ID

CODEOWNERS = ["@elektroleak"]
DEPENDENCIES = ["remote_receiver"]

rf6036b_ns = cg.esphome_ns.namespace("rf6036b")
Rf6036bComponent = rf6036b_ns.class_("Rf6036bComponent", cg.Component)

CONF_RECEIVER_ID = "receiver_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Rf6036bComponent),
        cv.Required(CONF_RECEIVER_ID): cv.use_id(remote_base.RemoteReceiverBase),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    receiver = await cg.get_variable(config[CONF_RECEIVER_ID])
    cg.add(var.set_receiver(receiver))
