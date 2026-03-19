// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include "device_agent.h"
#include "device_agent_settings_model.h"
#include "stub_analytics_plugin_mqtt_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx::vms_server_plugins::analytics::stub::mqtt {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Integration* integration):
    nx::sdk::analytics::Engine(ini().enableOutput, integration->instanceId()),
    m_integration(integration)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

std::string Engine::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*)R"json(
{
    "capabilities": "",
    "deviceAgentSettingsModel":
)json" + std::string(kDeviceAgentSettingsModel) + R"json(
}
)json";

    return result;
}

} // namespace nx::vms_server_plugins::analytics::stub::mqtt

