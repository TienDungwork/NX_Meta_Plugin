// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mqtt_config_store.h"

namespace nx::vms_server_plugins::analytics::stub::common {

MqttConfigStore& MqttConfigStore::instance()
{
    static MqttConfigStore g;
    return g;
}

void MqttConfigStore::set(MqttConfig cfg)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_cfg != cfg)
        {
            m_cfg = std::move(cfg);
            changed = true;
        }
    }
    if (changed)
        m_version.fetch_add(1);
}

MqttConfig MqttConfigStore::get() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cfg;
}

} // namespace nx::vms_server_plugins::analytics::stub::common

