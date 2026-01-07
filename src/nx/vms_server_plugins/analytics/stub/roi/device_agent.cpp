// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>
#include <nx/kit/utils.h>
#include <nx/kit/json.h>
#include <nx/sdk/helpers/settings_response.h>

#include "stub_analytics_plugin_roi_ini.h"
#include "http_server.h"
#include <mutex>

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

// Static member initialization
std::shared_ptr<HttpServer> DeviceAgent::m_httpServer = nullptr;
std::mutex DeviceAgent::m_httpServerMutex;
std::map<std::string, DeviceAgent*> DeviceAgent::m_deviceAgents;

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, NX_DEBUG_ENABLE_OUTPUT, engine->plugin()->instanceId()),
    m_engine(engine),
    m_cameraId(deviceInfo->id())
{
    NX_PRINT << "ROI DeviceAgent created with HTTP support";
    NX_PRINT << "Camera ID: " << m_cameraId;
    
    std::lock_guard<std::mutex> lock(m_httpServerMutex);
    
    // Register this device agent
    m_deviceAgents[m_cameraId] = this;
    
    // Create HTTP server once (shared by all cameras)
    if (!m_httpServer)
    {
        m_httpServer = std::make_shared<HttpServer>(8090);
        
        // Set callback to route requests to correct device agent
        m_httpServer->setRequestCallback(
            [](const std::string& cameraId) -> std::string {
                std::lock_guard<std::mutex> lock(m_httpServerMutex);
                auto it = m_deviceAgents.find(cameraId);
                if (it != m_deviceAgents.end())
                {
                    return it->second->handleHttpRequest(cameraId);
                }
                
                nx::kit::Json::object errorResponse;
                errorResponse["error"] = "Camera not found";
                errorResponse["camera_id"] = cameraId;
                return nx::kit::Json(errorResponse).dump();
            }
        );
        
        m_httpServer->start();
        NX_PRINT << "HTTP Server started on port 8090";
    }
}

DeviceAgent::~DeviceAgent()
{
    NX_PRINT << "ROI DeviceAgent destroyed";
    
    std::lock_guard<std::mutex> lock(m_httpServerMutex);
    
    // Unregister this device agent
    m_deviceAgents.erase(m_cameraId);
    
    // Stop HTTP server when last device agent is destroyed
    if (m_deviceAgents.empty() && m_httpServer)
    {
        m_httpServer->stop();
        m_httpServer.reset();
        NX_PRINT << "HTTP Server stopped";
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
    
    // Parse và LƯU polygon
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
    
    // LƯU polygon data vào memory (KHÔNG PUBLISH)
    if (!drawnPolygons.empty())
    {
        nx::kit::Json::object polygonData;
        polygonData["camera_id"] = m_cameraId;
        polygonData["timestamp"] = std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
        polygonData["polygons"] = drawnPolygons;
        
        m_storedPolygonData = nx::kit::Json(polygonData).dump();
        
        NX_PRINT << "Stored " << drawnPolygons.size() << " polygon(s) for camera: " << m_cameraId;
        NX_PRINT << "Waiting for MQTT request to send data...";
    }
    else
    {
        m_storedPolygonData.clear();
        NX_PRINT << "No drawn polygons - cleared stored data";
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

std::string DeviceAgent::getStoredPolygonData() const
{
    return m_storedPolygonData;
}

std::string DeviceAgent::handleHttpRequest(const std::string& cameraId)
{
    NX_PRINT << "========================================";
    NX_PRINT << "HTTP Request received:";
    NX_PRINT << "  Camera ID: " << cameraId;
    NX_PRINT << "  My Camera ID: " << m_cameraId;
    
    // Kiểm tra xem request có phải cho camera này không
    if (cameraId != m_cameraId)
    {
        NX_PRINT << "Request not for this camera - ignoring";
        NX_PRINT << "========================================";
        
        nx::kit::Json::object errorResponse;
        errorResponse["error"] = "Camera ID mismatch";
        errorResponse["requested"] = cameraId;
        errorResponse["actual"] = m_cameraId;
        return nx::kit::Json(errorResponse).dump();
    }
    
    // Response với stored polygon data
    if (!m_storedPolygonData.empty())
    {
        NX_PRINT << "Sending stored polygon data via HTTP response";
        NX_PRINT << "Response sent successfully";
        NX_PRINT << "========================================";
        return m_storedPolygonData;
    }
    else
    {
        // Gửi empty response
        nx::kit::Json::object emptyResponse;
        emptyResponse["camera_id"] = m_cameraId;
        emptyResponse["timestamp"] = std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
        emptyResponse["polygons"] = nx::kit::Json::array{};
        
        NX_PRINT << "No polygon data stored - sending empty response";
        NX_PRINT << "========================================";
        return nx::kit::Json(emptyResponse).dump();
    }
}

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
