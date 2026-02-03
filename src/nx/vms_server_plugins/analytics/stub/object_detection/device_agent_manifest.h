// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

static const std::string kDeviceAgentManifest = /*suppress newline*/ 1 + (const char*) R"json(
{
    "typeLibrary":
    {
        "objectTypes":
        [
            {
                "id": "nx.atin.counter",
                "name": "Số người vào"
            }
        ]
    },
    "supportedTypes":
    [
        {
            "objectTypeId": "nx.base.Face",
            "attributes":
            [
                "Gender",
                "Age",
                "Emotion",
                "Name"
            ]
        },
        {
            "objectTypeId": "nx.atin.counter",
            "attributes":
            [
                "Name",
                "counterText"
            ]
        }
    ]
}
)json";

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
