// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <algorithm>
#include <chrono>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>

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
                nx::sdk::Uuid trackId = trackIdByTrackIndex(detection.trackId - 1);
                objectMetadata->setTrackId(trackId);
                
                // Mark this track as needing Best Shot for Advanced Object Search
                // Only if we haven't generated Best Shot for this track yet
                if (m_trackIdsWithBestShot.find(trackId) == m_trackIdsWithBestShot.end())
                {
                    // Store bounding box for Best Shot generation
                    m_trackIdsNeedingBestShot[trackId] = boundingBox;
                }
                
                // Set confidence like fake generation (1.0 default)
                objectMetadata->setConfidence(detection.confidence);                
                // Valid colors: Magenta, Blue, Green, Yellow, Cyan, Purple, Orange, Red, White
                if (!detection.color.empty())
                {
                    objectMetadata->addAttribute(makePtr<Attribute>(
                        Attribute::Type::string,
                        "nx.sys.color",
                        detection.color));
                }
                
                // Add attributes if enabled (like fake generation)
                if (m_sendAttributes)
                {
                    objectMetadata->addAttribute(makePtr<Attribute>(
                        "confidence",
                        std::to_string(detection.confidence)));
                    
                    // Add name attribute if provided
                    if (!detection.name.empty())
                    {
                        objectMetadata->addAttribute(makePtr<Attribute>(
                            "name",
                            detection.name));
                    }
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
    
    // Add counter object to display "Số người vào: X"
    addCounterObject(metadataPacket, frameTimestampUs);

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
    
    std::string detectionsTopic = "vms/ai/detections/" + cameraId;
    std::string counterTopic = "vms/ai/counter/" + cameraId;  // Separate topic for counter
    
    NX_PRINT << "Camera ID: " << cameraId;
    NX_PRINT << "MQTT Detections Topic: " << detectionsTopic;
    NX_PRINT << "MQTT Counter Topic: " << counterTopic;
    
    // Initialize MQTT receiver for AI detections (bbox)
    m_mqttReceiver = std::make_unique<MqttObjectReceiver>("103.9.158.149", 1883, detectionsTopic);
    m_mqttReceiver->start();
    
    // Initialize MQTT receiver for people counter (totalCount) - separate topic
    m_mqttCounterReceiver = std::make_unique<MqttCounterReceiver>("103.9.158.149", 1883, counterTopic);
    m_mqttCounterReceiver->start();
}

DeviceAgent::~DeviceAgent()
{
    if (m_mqttReceiver)
    {
        m_mqttReceiver->stop();
    }
    if (m_mqttCounterReceiver)
    {
        m_mqttCounterReceiver->stop();
    }
}

std::string DeviceAgent::manifestString() const
{
    return kDeviceAgentManifest;
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame)
{
    ++m_frameIndex;
    if (m_trackIds.size() > 100)
    {
        m_trackIds.clear();
    }

    int64_t frameTimestampUs = videoFrame->timestampUs() + m_timestampShiftMs * 1000;
    
    Ptr<IMetadataPacket> objectMetadataPacket = generateObjectMetadataPacket(frameTimestampUs);
    pushMetadataPacket(objectMetadataPacket.releasePtr());

    // Generate and push Best Shot packets for Advanced Object Search
    std::vector<Ptr<IObjectTrackBestShotPacket>> bestShotPackets = generateBestShots(frameTimestampUs);
    for (Ptr<IObjectTrackBestShotPacket>& bestShotPacket: bestShotPackets)
        pushMetadataPacket(bestShotPacket.releasePtr());

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
    // Use map to avoid creating thousands of unused UUIDs
    auto it = m_trackIds.find(trackIndex);
    if (it != m_trackIds.end())
    {
        return it->second;
    }
    
    // Create new UUID for this track index
    Uuid newUuid = UuidHelper::randomUuid();
    m_trackIds[trackIndex] = newUuid;
    return newUuid;
}

void DeviceAgent::addCounterObject(Ptr<ObjectMetadataPacket> metadataPacket, int64_t frameTimestampUs)
{
    // Get totalCount from separate counter receiver (not from detections receiver)
    int totalCount = m_mqttCounterReceiver->getTotalCount();
    bool hasCounterData = m_mqttCounterReceiver->hasReceivedData();
    
    // Debug log
    if (m_frameIndex % 25 == 0)
    {
        NX_PRINT << "Counter check: totalCount=" << totalCount 
                 << ", hasReceivedData=" << (hasCounterData ? "true" : "false");
    }
    
    // Only show counter if we have a valid totalCount from MQTT
    if (totalCount < 0)
    {
        if (m_frameIndex % 25 == 0 && !hasCounterData)
        {
            NX_PRINT << "Counter not available - no data from counter receiver yet";
        }
        return;
    }
    
    // Create counter object metadata
    auto counterObject = makePtr<ObjectMetadata>();
    static const Uuid counterTrackId = UuidHelper::randomUuid();
    counterObject->setTypeId("nx.atin.counter");
    counterObject->setTrackId(counterTrackId);
    
    // Set bounding box rất nhỏ và ở ngoài màn hình để không hiển thị bbox
    // nhưng vẫn cho phép NX hiển thị text overlay
    Rect counterBbox;
    counterBbox.x = -0.1F;      // Ngoài màn hình bên trái
    counterBbox.y = -0.1F;      // Ngoài màn hình phía trên
    counterBbox.width = 0.001F;  // Rất nhỏ để không hiển thị
    counterBbox.height = 0.001F; // Rất nhỏ để không hiển thị
    counterObject->setBoundingBox(counterBbox);
    
    // Use "Name" attribute - NX often displays this as text overlay
    std::string counterText = "Số người vào: " + std::to_string(totalCount);
    counterObject->addAttribute(makePtr<Attribute>(
        Attribute::Type::string,
        "Name",  // Use "Name" attribute which NX may display
        counterText));
    
    // Also add counterText as backup
    counterObject->addAttribute(makePtr<Attribute>(
        Attribute::Type::string,
        "counterText",
        counterText));
    
    metadataPacket->addItem(counterObject.get());
    
    // Debug log every 25 frames (1 second at 25fps)
    if (m_frameIndex % 25 == 0)
    {
        NX_PRINT << "Counter object added: " << counterText 
                 << " (bbox: " << counterBbox.x << "," << counterBbox.y 
                 << " " << counterBbox.width << "x" << counterBbox.height << ")";
    }
}

std::vector<Ptr<IObjectTrackBestShotPacket>> DeviceAgent::generateBestShots(int64_t frameTimestampUs)
{
    std::vector<Ptr<IObjectTrackBestShotPacket>> result;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Generate Best Shot for all tracks that need it
    for (const auto& entry : m_trackIdsNeedingBestShot)
    {
        const Uuid& trackId = entry.first;
        const Rect& boundingBox = entry.second;
        
        // Create Best Shot packet with bounding box from detection
        auto bestShotPacket = makePtr<ObjectTrackBestShotPacket>(
            trackId,
            frameTimestampUs,
            boundingBox);
        
        result.push_back(std::move(bestShotPacket));
        
        // Mark this track as having Best Shot generated
        m_trackIdsWithBestShot.insert(trackId);
    }
    
    // Clear the map after generating Best Shots
    m_trackIdsNeedingBestShot.clear();
    
    return result;
}

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
