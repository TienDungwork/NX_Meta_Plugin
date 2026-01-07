// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <memory>
#include <string>

#include "engine.h"
#include "stub_analytics_plugin_roi_ini.h"
#include "http_server.h"

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


    std::string getCameraId() const { return m_cameraId; }
    std::string getStoredPolygonData() const;

protected:
    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual std::string manifestString() const override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    std::string handleHttpRequest(const std::string& cameraId);

private:
    Engine* const m_engine;
    std::string m_cameraId;
    std::string m_storedPolygonData;
    
    static std::shared_ptr<HttpServer> m_httpServer;
    static std::mutex m_httpServerMutex;
    static std::map<std::string, DeviceAgent*> m_deviceAgents;
};

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
