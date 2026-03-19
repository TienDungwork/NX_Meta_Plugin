// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms_server_plugins::analytics::stub::mqtt {

// A small settings UI just to configure MQTT connection parameters.
constexpr auto kDeviceAgentSettingsModel = R"json(
{
    "type": "Settings",
    "items": [
        {
            "type": "GroupBox",
            "caption": "MQTT",
            "items": [
                {
                    "type": "CheckBox",
                    "name": "mqtt.enabled",
                    "caption": "Enable MQTT",
                    "defaultValue": true
                },
                {
                    "type": "TextField",
                    "name": "mqtt.host",
                    "caption": "Broker host",
                    "defaultValue": "192.168.1.196"
                },
                {
                    "type": "SpinBox",
                    "name": "mqtt.port",
                    "caption": "Broker port",
                    "minValue": 1,
                    "maxValue": 65535,
                    "defaultValue": 1883
                },
                {
                    "type": "TextField",
                    "name": "mqtt.username",
                    "caption": "Username (optional)",
                    "defaultValue": ""
                },
                {
                    "type": "PasswordField",
                    "name": "mqtt.password",
                    "caption": "Password (optional)",
                    "defaultValue": ""
                }
            ]
        }
    ]
}
)json";

} // namespace nx::vms_server_plugins::analytics::stub::mqtt

