// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL 2.0/

#include "stub_analytics_plugin_mqtt_ini.h"

namespace nx::vms_server_plugins::analytics::stub::mqtt {

Ini& ini()
{
    static Ini ini;
    return ini;
}

} // namespace nx::vms_server_plugins::analytics::stub::mqtt

