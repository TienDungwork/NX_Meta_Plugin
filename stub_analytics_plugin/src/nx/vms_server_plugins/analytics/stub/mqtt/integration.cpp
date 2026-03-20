// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration.h"

#include "engine.h"

namespace nx::vms_server_plugins::analytics::stub::mqtt {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Integration::doObtainEngine()
{
    return new Engine(this);
}

std::string Integration::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "id": "atin.mqtt",
    "name": "MQTT Configuration",
    "description": "MQTT connection module for broker configuration shared across analytics integrations.",
    "version": "2.0.0",
    "vendor": "ATIN Advanced Technology Innovations"
}
)json";
}

} // namespace nx::vms_server_plugins::analytics::stub::mqtt

