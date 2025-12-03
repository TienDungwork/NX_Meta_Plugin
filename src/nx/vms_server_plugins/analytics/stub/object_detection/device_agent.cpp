// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <algorithm>
#include <chrono>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

#include "device_agent_manifest.h"
#include "object_attributes.h"
#include "../utils.h"
#include "stub_analytics_plugin_object_detection_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "[Object Detection] "
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using Uuid = nx::sdk::Uuid;

static constexpr int kTrackLength = 200;
static constexpr float kMaxBoundingBoxWidth = 0.5F;
static constexpr float kMaxBoundingBoxHeight = 0.5F;
static constexpr float kFreeSpace = 0.1F;
const std::string DeviceAgent::kTimeShiftSetting = "timestampShiftMs";
const std::string DeviceAgent::kSendAttributesSetting = "sendAttributes";
const std::string DeviceAgent::kObjectTypeGenerationSettingPrefix = "objectTypeIdToGenerate.";

static Rect generateBoundingBox(int frameIndex, int trackIndex, int trackCount)
{
    Rect boundingBox;
    boundingBox.width = std::min((1.0F - kFreeSpace) / trackCount, kMaxBoundingBoxWidth);
    boundingBox.height = std::min(boundingBox.width, kMaxBoundingBoxHeight);
    boundingBox.x = 1.0F / trackCount * trackIndex + kFreeSpace / (trackCount + 1);
    boundingBox.y = std::max(
        0.0F,
        1.0F - boundingBox.height - (1.0F / kTrackLength) * (frameIndex % kTrackLength));

    return boundingBox;
}

static std::vector<Ptr<ObjectMetadata>> generateObjects(
    const std::map<std::string, std::map<std::string, std::string>>& attributesByObjectType,
    const std::set<std::string>& objectTypeIdsToGenerate,
    bool doGenerateAttributes)
{
    std::vector<Ptr<ObjectMetadata>> result;

    for (const auto& entry: attributesByObjectType)
    {
        const std::string& objectTypeId = entry.first;
        if (objectTypeIdsToGenerate.find(objectTypeId) == objectTypeIdsToGenerate.cend())
            continue;

        auto objectMetadata = makePtr<ObjectMetadata>();
        objectMetadata->setTypeId(objectTypeId);

        if (doGenerateAttributes)
        {
            const std::map<std::string, std::string>& attributes = entry.second;
            for (const auto& attribute: attributes)
                objectMetadata->addAttribute(makePtr<Attribute>(attribute.first, attribute.second));
        }

        result.push_back(std::move(objectMetadata));
    }

    return result;
}

Ptr<IMetadataPacket> DeviceAgent::generateObjectMetadataPacket(int64_t frameTimestampUs)
{
    auto metadataPacket = makePtr<ObjectMetadataPacket>();
    metadataPacket->setTimestampUs(frameTimestampUs);

    // Check if MQTT is active (ever received any message)
    bool hasMqttConnection = m_mqttReceiver->hasReceivedData();
    std::vector<DetectedObject> mqttDetections = m_mqttReceiver->getAndClearDetectedObjects();
    
    if (hasMqttConnection)
    {
        // MQTT is active - use MQTT data or show nothing
        if (!mqttDetections.empty())
        {
            // USE MQTT DETECTIONS
            std::lock_guard<std::mutex> lock(m_mutex);
            
            NX_PRINT << "Using MQTT detections: " << mqttDetections.size() << " objects";
            
            // Store objects in vector to prevent destruction before addItem
            std::vector<Ptr<ObjectMetadata>> mqttObjects;
            
            for (const auto& detection : mqttDetections)
            {
                // Map MQTT label to VMS object type ID
                std::string objectTypeId;
                std::string label = detection.label;
                
                // Convert label to lowercase for comparison
                std::transform(label.begin(), label.end(), label.begin(), ::tolower);
                
                // Capitalize first letter for nx.base format
                std::string capitalizedLabel = label;
                if (!capitalizedLabel.empty())
                    capitalizedLabel[0] = std::toupper(capitalizedLabel[0]);
                
                // All labels use nx.base.{Label} format
                objectTypeId = "nx.base." + capitalizedLabel;
                
                NX_PRINT << "MQTT label '" << label << "' -> object type: " << objectTypeId;
                
                // Check if this object type is enabled in settings
                if (!m_objectTypeIdsToGenerate.empty() && 
                    m_objectTypeIdsToGenerate.find(objectTypeId) == m_objectTypeIdsToGenerate.end())
                {
                    continue; // Skip this object if not enabled
                }
                
                auto objectMetadata = makePtr<ObjectMetadata>();
                objectMetadata->setTypeId(objectTypeId);
                
                // Set bounding box from detection
                Rect boundingBox;
                boundingBox.x = detection.x;
                boundingBox.y = detection.y;
                boundingBox.width = detection.width;
                boundingBox.height = detection.height;
                objectMetadata->setBoundingBox(boundingBox);
                
                // Set track ID from detection (MUST use UUID like fake generation)
                objectMetadata->setTrackId(trackIdByTrackIndex(detection.trackId - 1));
                
                // Set confidence like fake generation (1.0 default)
                objectMetadata->setConfidence(detection.confidence);
                
                // Add attributes if enabled (like fake generation)
                if (m_sendAttributes)
                {
                    objectMetadata->addAttribute(makePtr<Attribute>(
                        "confidence",
                        std::to_string(detection.confidence)));
                }
                
                mqttObjects.push_back(std::move(objectMetadata));
            }
            
            // Add all MQTT objects to packet (IMPORTANT: do this AFTER loop)
            for (const auto& obj : mqttObjects)
            {
                metadataPacket->addItem(obj.get());
            }
            
            NX_PRINT << "Added " << mqttObjects.size() << " MQTT objects to packet";
        }
        else
        {
            // MQTT is active but sent empty detections - show nothing
            NX_PRINT << "MQTT active but empty detections - showing nothing";
            // Don't add any items to metadataPacket
        }
    }
    else
    {
        // NO MQTT CONNECTION - KHÔNG HIỂN THỊ GÌ (fake bbox đã TẮT)
        // Không thêm objects nào vào metadataPacket
    }

    return metadataPacket;
}

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, ini().enableOutput)
{
    // Get camera ID and create topic specific to this camera
    std::string cameraId = deviceInfo->id();
    
    // Remove curly braces from UUID if present
    if (!cameraId.empty() && cameraId.front() == '{')
        cameraId = cameraId.substr(1, cameraId.length() - 2);
    
    std::string topic = "vms/ai/detections/" + cameraId;
    
    NX_PRINT << "Camera ID: " << cameraId;
    NX_PRINT << "MQTT Topic: " << topic;
    
    // Initialize MQTT receiver to get AI detections for this specific camera
    m_mqttReceiver = std::make_unique<MqttObjectReceiver>("192.168.1.215", 1883, topic);
    m_mqttReceiver->start();
}

DeviceAgent::~DeviceAgent()
{
    if (m_mqttReceiver)
    {
        m_mqttReceiver->stop();
    }
}

std::string DeviceAgent::manifestString() const
{
    return kDeviceAgentManifest;
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    ++m_frameIndex;
    if ((m_frameIndex % kTrackLength) == 0)
        m_trackIds.clear();

    Ptr<IMetadataPacket> objectMetadataPacket = generateObjectMetadataPacket(
        videoFrame->timestampUs() + m_timestampShiftMs * 1000);

    pushMetadataPacket(objectMetadataPacket.releasePtr());

    return true;
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

nx::sdk::Result<const nx::sdk::ISettingsResponse*> DeviceAgent::settingsReceived()
{
    const std::lock_guard<std::mutex> lock(m_mutex);

    m_objectTypeIdsToGenerate.clear();

    const std::map<std::string, std::string>& settings = currentSettings();
    for (const auto& entry: settings)
    {
        const std::string& key = entry.first;
        const std::string& value = entry.second;
        if (startsWith(key, kObjectTypeGenerationSettingPrefix) && toBool(value))
        {
            std::string objectType = key.substr(kObjectTypeGenerationSettingPrefix.size());
            m_objectTypeIdsToGenerate.insert(objectType);
            NX_PRINT << "Enabled object type: " << objectType;
        }
        else if (key == kSendAttributesSetting)
            m_sendAttributes = toBool(value);
        else if (key == kTimeShiftSetting)
            m_timestampShiftMs = std::stoi(value);
    }
    
    NX_PRINT << "Total enabled object types: " << m_objectTypeIdsToGenerate.size();

    return nullptr;
}

Uuid DeviceAgent::trackIdByTrackIndex(int trackIndex)
{
    while (trackIndex >= m_trackIds.size())
        m_trackIds.push_back(UuidHelper::randomUuid());

    return m_trackIds[trackIndex];
}

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
