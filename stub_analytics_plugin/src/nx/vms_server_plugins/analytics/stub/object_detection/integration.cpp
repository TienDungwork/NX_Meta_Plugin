// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration.h"

#include <nx/kit/utils.h>

#include "engine.h"
#include "stub_analytics_plugin_object_detection_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Result<IEngine*> Integration::doObtainEngine()
{
    return new Engine();
}

std::string Integration::manifestString() const
{
    const static std::string manifest = /*suppress newline*/ 1 + (const char*) R"json(
    {
        "id": "atin.object_detection",
        "name": "Object Detection",
        "description": "AI object detection integration optimized for stable real-time metadata streaming over MQTT.",
        "version": "2.0.0",
        "vendor": "ATIN Advanced Technology Innovations",
        "isLicenseRequired": %s
    }
    )json";

    return nx::kit::utils::format(manifest, ini().isLicenseRequired ? "true" : "false");
}

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
