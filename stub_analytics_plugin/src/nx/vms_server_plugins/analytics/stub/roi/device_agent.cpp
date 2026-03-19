// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <nx/kit/utils.h>
#include <nx/kit/json.h>
#include <nx/sdk/helpers/settings_response.h>

#include "../common/polygon_http_api.h"
#include "../common/roi_store.h"

#include "device_agent_settings_model.h"
#include "stub_analytics_plugin_roi_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->integration()->instanceId()),
    m_engine(engine)
{
    if (deviceInfo && deviceInfo->id())
        m_deviceId = deviceInfo->id();
    if (!m_deviceId.empty() && m_deviceId.front() == '{')
        m_deviceId = m_deviceId.substr(1, m_deviceId.size() - 2);
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
    const auto allSettings = currentSettings(); //< provided by ConsumingDeviceAgent

    // Snapshot per camera for HTTP /polygon consumers.
    nx::vms_server_plugins::analytics::stub::common::RoiStore::instance().setSettingsForDevice(
        m_deviceId, allSettings);

    // As requested: HTTP polygon API is always enabled (no UI toggle).
    // Default port chosen to match your scripts.
    nx::vms_server_plugins::analytics::stub::common::startPolygonHttpApi(/*port*/ 8090);

    return nullptr;
}

void DeviceAgent::getIntegrationSideSettings(
    Result<const ISettingsResponse*>* outResult) const
{
    // No integration-side settings in this trimmed ROI-only variant.
    (void) outResult;
}

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
