// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <mutex>
#include <string>

namespace nx::vms_server_plugins::analytics::stub::common {

struct MqttConfig
{
    bool enabled = true;
    std::string host = "103.9.156.56";
    int port = 1883;
    std::string username;
    std::string password;

    bool operator==(const MqttConfig& other) const
    {
        return enabled == other.enabled
            && host == other.host
            && port == other.port
            && username == other.username
            && password == other.password;
    }

    bool operator!=(const MqttConfig& other) const { return !(*this == other); }
};

class MqttConfigStore
{
public:
    static MqttConfigStore& instance();

    void set(MqttConfig cfg);
    MqttConfig get() const;

    /** Increments on every set(). Used by consumers to detect changes. */
    uint64_t version() const { return m_version.load(); }

private:
    MqttConfigStore() = default;

private:
    mutable std::mutex m_mutex;
    MqttConfig m_cfg;
    std::atomic<uint64_t> m_version{1};
};

} // namespace nx::vms_server_plugins::analytics::stub::common

