// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

static const std::string kDeviceAgentSettingsModel = /*suppress newline*/ 1 + R"json(
{
    "type": "Settings",
    "items":
    [
        {
            "type": "GroupBox",
            "caption": "Detection area",
            "items":
            [
                {
                    "type": "PolygonFigure",
                    "name": "excludedArea.figure",
                    "caption": "Detection area",
                    "useLabelField": false,
                    "maxPoints": 8
                }
            ]
        }
    ]
}
)json";

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
