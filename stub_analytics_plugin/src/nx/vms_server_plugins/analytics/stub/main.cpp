// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/kit/debug.h>

#include "object_detection/integration.h"
#include "roi/integration.h"
#include "mqtt/integration.h"

extern "C" NX_PLUGIN_API nx::sdk::IIntegration* createNxPluginByIndex(int instanceIndex)
{
    using namespace nx::vms_server_plugins::analytics::stub;

    switch (instanceIndex)
    {
        case 0: return new roi::Integration();
        case 1: return new object_detection::Integration();
        case 2: return new mqtt::Integration();
        default: return nullptr;
    }
}
