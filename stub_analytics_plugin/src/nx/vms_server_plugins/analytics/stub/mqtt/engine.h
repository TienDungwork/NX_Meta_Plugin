// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/analytics/helpers/integration.h>

namespace nx::sdk { class IDeviceInfo; }
namespace nx::sdk::analytics { class IDeviceAgent; }

namespace nx::vms_server_plugins::analytics::stub::mqtt {

class Engine: public nx::sdk::analytics::Engine
{
public:
    explicit Engine(nx::sdk::analytics::Integration* integration);
    ~Engine() override;

    nx::sdk::analytics::Integration* const integration() const { return m_integration; }

protected:
    std::string manifestString() const override;

    void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;

private:
    nx::sdk::analytics::Integration* const m_integration;
};

} // namespace nx::vms_server_plugins::analytics::stub::mqtt

