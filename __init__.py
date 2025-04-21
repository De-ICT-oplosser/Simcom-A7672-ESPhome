from esphome import automation
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MESSAGE, CONF_TRIGGER_ID

# Define dependency as uart
DEPENDENCIES = ["uart"]
# Update CODEOWNERS if you plan to share/publish
CODEOWNERS = ["@your_github_username"] # Optional: Change to your username
MULTI_CONF = True

# Renamed namespace and class names
a7672_ns = cg.esphome_ns.namespace("a7672")
A7672Component = a7672_ns.class_("A7672Component", cg.Component, uart.UARTDevice) # Inherit from UARTDevice

A7672ReceivedMessageTrigger = a7672_ns.class_(
    "A7672ReceivedMessageTrigger",
    automation.Trigger.template(cg.std_string, cg.std_string),
)
A7672IncomingCallTrigger = a7672_ns.class_(
    "A7672IncomingCallTrigger",
    automation.Trigger.template(cg.std_string),
)
A7672CallConnectedTrigger = a7672_ns.class_(
    "A7672CallConnectedTrigger",
    automation.Trigger.template(),
)
A7672CallDisconnectedTrigger = a7672_ns.class_(
    "A7672CallDisconnectedTrigger",
    automation.Trigger.template(),
)

A7672ReceivedUssdTrigger = a7672_ns.class_(
    "A7672ReceivedUssdTrigger",
    automation.Trigger.template(cg.std_string),
)

# Renamed Actions
A7672SendSmsAction = a7672_ns.class_("A7672SendSmsAction", automation.Action)
A7672SendUssdAction = a7672_ns.class_("A7672SendUssdAction", automation.Action)
A7672DialAction = a7672_ns.class_("A7672DialAction", automation.Action)
A7672ConnectAction = a7672_ns.class_("A7672ConnectAction", automation.Action)
A7672DisconnectAction = a7672_ns.class_(
    "A7672DisconnectAction", automation.Action
)

# Renamed ID constant
CONF_A7672_ID = "a7672_id"
# Keep other CONF constants the same as they define YAML keys users will use
CONF_ON_SMS_RECEIVED = "on_sms_received"
CONF_ON_USSD_RECEIVED = "on_ussd_received"
CONF_ON_INCOMING_CALL = "on_incoming_call"
CONF_ON_CALL_CONNECTED = "on_call_connected"
CONF_ON_CALL_DISCONNECTED = "on_call_disconnected"
CONF_RECIPIENT = "recipient"
CONF_USSD = "ussd"

CONFIG_SCHEMA = cv.All(
    # Define the schema for the main component entry (e.g., `a7672:`)
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(A7672Component), # Use renamed component
            # Trigger configurations
            cv.Optional(CONF_ON_SMS_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7672ReceivedMessageTrigger # Use renamed trigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_INCOMING_CALL): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7672IncomingCallTrigger # Use renamed trigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_CALL_CONNECTED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7672CallConnectedTrigger # Use renamed trigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_CALL_DISCONNECTED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        A7672CallDisconnectedTrigger # Use renamed trigger
                    ),
                }
            ),
             cv.Optional(CONF_ON_USSD_RECEIVED): automation.validate_automation(
                 {
                     cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                         A7672ReceivedUssdTrigger # Use renamed trigger
                     ),
                 }
             ),
        }
    )
    .extend(cv.polling_component_schema("5s")) # Keep polling schema
    .extend(uart.UART_DEVICE_SCHEMA) # Keep UART schema
)

# Final validation using the new component name "a7672"
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "a7672", require_tx=True, require_rx=True
)


async def to_code(config):
    # Generate code for the main component
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Generate code for triggers
    for conf in config.get(CONF_ON_SMS_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(cg.std_string, "message"), (cg.std_string, "sender")], conf
        )
    for conf in config.get(CONF_ON_INCOMING_CALL, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "caller_id")], conf)
    for conf in config.get(CONF_ON_CALL_CONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CALL_DISCONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_USSD_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "ussd")], conf)


# Schema for the send_sms action
A7672_SEND_SMS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(A7672Component), # Use renamed component ID
        cv.Required(CONF_RECIPIENT): cv.templatable(cv.string_strict),
        cv.Required(CONF_MESSAGE): cv.templatable(cv.string),
    }
)

# Register the send_sms action with the new name "a7672.send_sms"
@automation.register_action(
    "a7672.send_sms", A7672SendSmsAction, A7672_SEND_SMS_SCHEMA # Use renamed action/schema
)
async def a7672_send_sms_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_RECIPIENT], args, cg.std_string)
    cg.add(var.set_recipient(template_))
    template_ = await cg.templatable(config[CONF_MESSAGE], args, cg.std_string)
    cg.add(var.set_message(template_))
    return var

# Schema for the dial action
A7672_DIAL_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(A7672Component), # Use renamed component ID
        cv.Required(CONF_RECIPIENT): cv.templatable(cv.string_strict),
    }
)

# Register the dial action with the new name "a7672.dial"
@automation.register_action("a7672.dial", A7672DialAction, A7672_DIAL_SCHEMA) # Use renamed action/schema
async def a7672_dial_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_RECIPIENT], args, cg.std_string)
    cg.add(var.set_recipient(template_))
    return var

# Register the connect action with the new name "a7672.connect"
@automation.register_action(
    "a7672.connect",
    A7672ConnectAction, # Use renamed action
    cv.Schema({cv.GenerateID(): cv.use_id(A7672Component)}), # Use renamed component ID
)
async def a7672_connect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var

# Schema for the send_ussd action
A7672_SEND_USSD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(A7672Component), # Use renamed component ID
        cv.Required(CONF_USSD): cv.templatable(cv.string_strict),
    }
)

# Register the send_ussd action with the new name "a7672.send_ussd"
@automation.register_action(
    "a7672.send_ussd", A7672SendUssdAction, A7672_SEND_USSD_SCHEMA # Use renamed action/schema
)
async def a7672_send_ussd_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_USSD], args, cg.std_string)
    cg.add(var.set_ussd(template_))
    return var

# Register the disconnect action with the new name "a7672.disconnect"
@automation.register_action(
    "a7672.disconnect",
    A7672DisconnectAction, # Use renamed action
    cv.Schema({cv.GenerateID(): cv.use_id(A7672Component)}), # Use renamed component ID
)
async def a7672_disconnect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var