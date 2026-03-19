// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx::vms_server_plugins::analytics::stub::roi {

// As requested: keep ONLY "Excluded area" in the UI.
static const std::string kDeviceAgentSettingsModel = /*suppress newline*/ 1 + R"json(
{
    "type": "Settings",
    "items":
    [
        {
            "type": "PolygonFigure",
            "name": "excludedArea.figure",
            "caption": "Excluded area",
            "useLabelField": false,
            "maxPoints": 8
        }
    ]
}
)json";

} // namespace nx::vms_server_plugins::analytics::stub::roi
