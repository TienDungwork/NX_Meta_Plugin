// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <mutex>
#include <string>

namespace nx::vms_server_plugins::analytics::stub::common {

/**
 * Thread-safe store for ROI settings per camera (device id).
 * Values are the raw settings map coming from VMS (string->string), where non-string controls
 * are represented as JSON strings (see SDK settings_model.md).
 */
class RoiStore
{
public:
    static RoiStore& instance();

    void setSettingsForDevice(std::string deviceId, std::map<std::string, std::string> settings);
    std::map<std::string, std::string> settingsForDevice(const std::string& deviceId) const;

private:
    RoiStore() = default;

private:
    mutable std::mutex m_mutex;
    std::map<std::string, std::map<std::string, std::string>> m_settingsByDevice;
};

} // namespace nx::vms_server_plugins::analytics::stub::common

