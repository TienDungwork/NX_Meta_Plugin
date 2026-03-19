// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <nx/sdk/analytics/i_object_metadata.h>

namespace nx::vms_server_plugins::analytics::stub::common {

struct ExternalBox
{
    nx::sdk::analytics::Rect rect;     //< normalized [0..1]
    std::string typeId = "nx.base.Person";
    std::string trackId;               //< optional; if empty, plugin will generate per-type
    double confidence = 1.0;
};

class ExternalOverlays
{
public:
    static ExternalOverlays& instance();

    void setBoxes(std::vector<ExternalBox> boxes);
    std::vector<ExternalBox> boxesSnapshot() const;

    void setRoiSettings(std::map<std::string, std::string> settings);
    std::map<std::string, std::string> roiSettingsSnapshot() const;

private:
    ExternalOverlays() = default;

private:
    mutable std::mutex m_mutex;
    std::vector<ExternalBox> m_boxes;
    std::map<std::string, std::string> m_roiSettings;
};

} // namespace nx::vms_server_plugins::analytics::stub::common

