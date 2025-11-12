// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <memory>

#include "engine.h"
#include "stub_analytics_plugin_roi_ini.h"
#include "mqtt_publisher.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual std::string manifestString() const override;

    /**
     * Called when settings are received from VMS Client
     * Override để publish polygon qua MQTT (KHÔNG SỬA LOGIC CŨ)
     */
    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    Engine* const m_engine;
    std::unique_ptr<MqttPublisher> m_mqttPublisher;
};

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
