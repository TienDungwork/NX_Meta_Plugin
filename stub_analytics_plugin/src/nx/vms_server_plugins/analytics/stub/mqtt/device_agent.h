// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>

#include "engine.h"
#include "stub_analytics_plugin_mqtt_ini.h"

namespace nx::vms_server_plugins::analytics::stub::mqtt {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    ~DeviceAgent() override;

protected:
    nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

    void getIntegrationSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;

    std::string manifestString() const override;

private:
    Engine* const m_engine;
};

} // namespace nx::vms_server_plugins::analytics::stub::mqtt

