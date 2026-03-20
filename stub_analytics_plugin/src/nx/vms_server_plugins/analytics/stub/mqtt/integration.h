// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/integration.h>
#include <nx/sdk/analytics/i_engine.h>

namespace nx::vms_server_plugins::analytics::stub::mqtt {

class Integration: public nx::sdk::analytics::Integration
{
public:
    Integration() = default;

protected:
    nx::sdk::Result<nx::sdk::analytics::IEngine*> doObtainEngine() override;
    std::string instanceId() const override { return "atin.mqtt"; }
    std::string manifestString() const override;
};

} // namespace nx::vms_server_plugins::analytics::stub::mqtt

