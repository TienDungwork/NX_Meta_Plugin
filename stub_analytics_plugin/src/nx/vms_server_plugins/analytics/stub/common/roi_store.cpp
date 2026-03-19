// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "roi_store.h"

namespace nx::vms_server_plugins::analytics::stub::common {

RoiStore& RoiStore::instance()
{
    static RoiStore g;
    return g;
}

void RoiStore::setSettingsForDevice(std::string deviceId, std::map<std::string, std::string> settings)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settingsByDevice[std::move(deviceId)] = std::move(settings);
}

std::map<std::string, std::string> RoiStore::settingsForDevice(const std::string& deviceId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_settingsByDevice.find(deviceId);
    if (it == m_settingsByDevice.end())
        return {};
    return it->second;
}

} // namespace nx::vms_server_plugins::analytics::stub::common

