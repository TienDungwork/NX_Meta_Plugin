// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "external_overlays.h"

namespace nx::vms_server_plugins::analytics::stub::common {

ExternalOverlays& ExternalOverlays::instance()
{
    static ExternalOverlays g;
    return g;
}

void ExternalOverlays::setBoxes(std::vector<ExternalBox> boxes)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_boxes = std::move(boxes);
}

std::vector<ExternalBox> ExternalOverlays::boxesSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_boxes;
}

void ExternalOverlays::setRoiSettings(std::map<std::string, std::string> settings)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_roiSettings = std::move(settings);
}

std::map<std::string, std::string> ExternalOverlays::roiSettingsSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_roiSettings;
}

} // namespace nx::vms_server_plugins::analytics::stub::common

