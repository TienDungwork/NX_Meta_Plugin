// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>
#include <nx/kit/utils.h>
#include <nx/kit/json.h>
#include <nx/sdk/helpers/settings_response.h>

#include "stub_analytics_plugin_roi_ini.h"
#include "mqtt_publisher.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->plugin()->instanceId()),
    m_engine(engine),
    m_mqttPublisher(new MqttPublisher())
{
    NX_PRINT << "ROI DeviceAgent created with MQTT support";
    // Start MQTT publisher
    m_mqttPublisher->start();
}

DeviceAgent::~DeviceAgent()
{
    NX_PRINT << "ROI DeviceAgent destroyed";
    if (m_mqttPublisher)
    {
        m_mqttPublisher->stop();
        m_mqttPublisher.reset();
    }
}

std::string DeviceAgent::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "capabilities": "disableStreamSelection"
}
)json";
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* /*outResult*/, const IMetadataTypes* neededMetadataTypes)
{
}

Result<const ISettingsResponse*> DeviceAgent::settingsReceived()
{
    NX_PRINT << "========================================";
    NX_PRINT << "settingsReceived() called - User changed settings!";
    
    // Đọc tất cả settings hiện tại từ VMS
    std::map<std::string, std::string> allSettings = currentSettings();
    
    NX_PRINT << "Total settings received: " << allSettings.size();
    
    // Publish
    if (m_mqttPublisher)
    {
        nx::kit::Json::array drawnPolygons;
        
        std::vector<std::string> targetPolygons = {"excludedArea.figure"};
        
        for (const std::string& polygonName : targetPolygons)
        {
            auto it = allSettings.find(polygonName);
            if (it != allSettings.end() && !it->second.empty())
            {
                std::string parseError;
                nx::kit::Json parsedValue = nx::kit::Json::parse(it->second, parseError);
                
                if (parseError.empty() && parsedValue.is_object())
                {
                    auto obj = parsedValue.object_items();
                    
                    // Kiểm tra figure và points
                    if (obj.count("figure") > 0 && !obj["figure"].is_null())
                    {
                        auto figure = obj["figure"];
                        if (figure.is_object())
                        {
                            auto figureObj = figure.object_items();
                            if (figureObj.count("points") > 0 && 
                                figureObj["points"].is_array() &&
                                !figureObj["points"].array_items().empty())
                            {
                                // Polygon
                                nx::kit::Json::object polygonInfo;
                                polygonInfo["name"] = polygonName;
                                polygonInfo["points"] = figureObj["points"];
                                polygonInfo["color"] = figureObj.count("color") > 0 
                                    ? figureObj["color"] : nx::kit::Json("#ffffff");
                                polygonInfo["label"] = obj.count("label") > 0 
                                    ? obj["label"] : nx::kit::Json("");
                                polygonInfo["showOnCamera"] = obj.count("showOnCamera") > 0 
                                    ? obj["showOnCamera"] : nx::kit::Json(false);
                                
                                drawnPolygons.push_back(polygonInfo);
                                
                                NX_PRINT << "  Found drawn polygon: " << polygonName 
                                         << " with " << figureObj["points"].array_items().size() 
                                         << " points";
                            }
                        }
                    }
                }
            }
        }
        
        if (!drawnPolygons.empty())
        {
            nx::kit::Json::object mqttPayload;
            mqttPayload["event"] = "polygons_updated";
            mqttPayload["timestamp"] = std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count());
            mqttPayload["polygons"] = drawnPolygons;
            
            std::string mqttMessage = nx::kit::Json(mqttPayload).dump();
            
            NX_PRINT << "Publishing " << drawnPolygons.size() << " polygon(s) to MQTT";
            m_mqttPublisher->publishPolygon(mqttMessage);
        }
        else
        {
            NX_PRINT << "No drawn polygons (testPolygon/excludedArea) - skipping MQTT";
        }
    }
    else
    {
        NX_PRINT << "WARNING: MQTT Publisher not initialized!";
    }
    
    NX_PRINT << "========================================";
    
    return nullptr;
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* outResult) const
{
    const auto response = new SettingsResponse();

    nx::kit::Json::array jsonPoints{
        nx::kit::Json::array{0.138, 0.551},
        nx::kit::Json::array{0.775, 0.429},
        nx::kit::Json::array{0.748, 0.844}};

    nx::kit::Json::object jsonFigure;
    jsonFigure.insert(std::make_pair("points", jsonPoints));

    nx::kit::Json::object jsonResult;
    jsonResult.insert(std::make_pair("figure", jsonFigure));

    response->setValue("testPolygon", nx::kit::Json(jsonResult).dump());
    *outResult = response;
}

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
