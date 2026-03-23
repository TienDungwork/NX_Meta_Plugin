// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"
#include "device_agent.h"
#include "device_agent_manifest.h"
#include "stub_analytics_plugin_object_detection_ini.h"

#include <nx/kit/json.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

static const std::set<std::string> kObjectTypeIdsGeneratedByDefault = {
    "nx.base.Vehicle",
    "nx.base.Face",
    "nx.base.Person",
    "nx.base.Unknown",
};

Engine::Engine(): nx::sdk::analytics::Engine(ini().enableOutput)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(deviceInfo);
}

std::string Engine::manifestString() const
{
    using namespace nx::kit;

    std::string errors;
    Json deviceAgentManifest = Json::parse(kDeviceAgentManifest, errors).object_items();

    Json::array generationSettings;

    Json::object timeShiftSetting = {
        {"type", "SpinBox"},
        {"name", DeviceAgent::kTimeShiftSetting},
        {"caption", "Timestamp shift"},
        {"description", "Metadata timestamp shift in milliseconds"},
        {"defaultValue", 0}
    };
    generationSettings.push_back(std::move(timeShiftSetting));

    Json::object attributesSetting = {
        {"type", "CheckBox"},
        {"name", DeviceAgent::kSendAttributesSetting},
        {"caption", "Send object attributes"},
        // Attributes generation can be relatively expensive. Default to disabled
        // so the stub can keep up with real-time frame ingestion.
        {"defaultValue", false}
    };
    generationSettings.push_back(std::move(attributesSetting));

    generationSettings.push_back(Json::object{ {"type", "Separator"} });

    for (const auto& supportedType : deviceAgentManifest["supportedTypes"].array_items())
    {
        Json::object supportedTypeObject = supportedType.object_items();
        const std::string& objectTypeId = supportedTypeObject["objectTypeId"].string_value();
        std::string caption = objectTypeId;
        if (objectTypeId == "nx.base.Face")
            caption = "Detect Face";
        else if (objectTypeId == "nx.base.Person")
            caption = "Detect Intrusion (People)";
        else if (objectTypeId == "nx.base.Vehicle")
            caption = "Detect Vehicle";
        else if (objectTypeId == "nx.base.Unknown")
            caption = "Detect Fire & Smoke";

        Json::object generationSetting = {
            {"type", "CheckBox"},
            {"name", DeviceAgent::kObjectTypeGenerationSettingPrefix + objectTypeId},
            {"caption", caption},
            {
                "defaultValue",
                kObjectTypeIdsGeneratedByDefault.find(objectTypeId)
                    != kObjectTypeIdsGeneratedByDefault.cend()
            }
        };

        generationSettings.push_back(std::move(generationSetting));
    }

    Json::object settingsModel = {
        {"type", "Settings"},
        {"items", generationSettings}
    };

    Json::object engineManifest = {
        {"capabilities", "updateMotionSensitivity"},
        {"streamTypeFilter", "compressedVideo"},
        {"deviceAgentSettingsModel", settingsModel}
    };

    return Json(engineManifest).dump();
}

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
