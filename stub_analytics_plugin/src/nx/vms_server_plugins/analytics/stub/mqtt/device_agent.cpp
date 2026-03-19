// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include "../common/mqtt_config_store.h"

namespace nx::vms_server_plugins::analytics::stub::mqtt {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine, const IDeviceInfo* deviceInfo):
    nx::sdk::analytics::ConsumingDeviceAgent(deviceInfo, ini().enableOutput, engine->integration()->instanceId()),
    m_engine(engine)
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "capabilities": "disableStreamSelection"
}
)json";
}

nx::sdk::Result<const nx::sdk::ISettingsResponse*> DeviceAgent::settingsReceived()
{
    const std::map<std::string, std::string>& s = currentSettings();

    common::MqttConfig cfg;
    if (auto it = s.find("mqtt.enabled"); it != s.end())
        cfg.enabled = (it->second == "true" || it->second == "1" || it->second == "yes" || it->second == "on");
    if (auto it = s.find("mqtt.host"); it != s.end() && !it->second.empty())
        cfg.host = it->second;
    if (auto it = s.find("mqtt.port"); it != s.end())
    {
        try { cfg.port = std::stoi(it->second); } catch (...) { /*keep default*/ }
    }
    if (auto it = s.find("mqtt.username"); it != s.end())
        cfg.username = it->second;
    if (auto it = s.find("mqtt.password"); it != s.end())
        cfg.password = it->second;

    common::MqttConfigStore::instance().set(std::move(cfg));
    return nullptr;
}

void DeviceAgent::getIntegrationSideSettings(Result<const ISettingsResponse*>* outResult) const
{
    // No integration-side settings for this helper stub.
    (void) outResult;
}

} // namespace nx::vms_server_plugins::analytics::stub::mqtt

