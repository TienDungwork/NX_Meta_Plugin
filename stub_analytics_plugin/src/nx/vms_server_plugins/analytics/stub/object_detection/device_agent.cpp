// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>
#include <nx/kit/json.h>

#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

#include "device_agent_manifest.h"
#include "object_attributes.h"
#include "../utils.h"
#include "../common/mqtt_config_store.h"
#include "stub_analytics_plugin_object_detection_ini.h"

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

    // Prefer MQTT-fed detections if present for this camera.
    if (m_mqttSubscriber)
    {
        const std::string payload = m_mqttSubscriber->takeLastPayload();
        if (!payload.empty())
        {
            m_hasReceivedMqtt = true;

            std::string parseError;
            const nx::kit::Json data = nx::kit::Json::parse(payload, parseError);
            if (parseError.empty() && data.is_object() && data["detections"].is_array())
            {
                for (const auto& det : data["detections"].array_items())
                {
                    if (!det.is_object())
                        continue;

                    const std::string label =
                        det["label"].is_string() ? det["label"].string_value() : "";
                    const double confidence =
                        det["confidence"].is_number() ? det["confidence"].number_value() : 1.0;
                    const int trackId =
                        det["trackId"].is_number() ? det["trackId"].int_value() : 0;

                    float x = 0, y = 0, w = 0, h = 0;
                    if (det["bbox"].is_array())
                    {
                        const auto bbox = det["bbox"].array_items();
                        if (bbox.size() >= 4)
                        {
                            x = (float) bbox[0].number_value();
                            y = (float) bbox[1].number_value();
                            w = (float) bbox[2].number_value();
                            h = (float) bbox[3].number_value();
                        }
                    }

                    auto obj = makePtr<ObjectMetadata>();

                    // Minimal label->typeId mapping. (Bạn có thể map lại theo taxonomy bạn muốn.)
                    std::string typeId = "nx.base.Person";
                    if (!label.empty())
                        typeId = "nx.base." + label; //< "Face" -> "nx.base.Face"

                    obj->setTypeId(typeId);
                    obj->setBoundingBox(Rect{x, y, w, h});
                    obj->setConfidence(confidence);
                    obj->setTrackId(trackIdByTrackType(typeId + "#" + std::to_string(trackId)));
                    metadataPacket->addItem(obj);
                }

                return metadataPacket;
            }
        }
    }
    // As requested: do NOT generate any sample boxes. Only show boxes coming from MQTT.
    return nullptr;
}

void DeviceAgent::restartMqttSubscriber(
    const nx::vms_server_plugins::analytics::stub::common::MqttConfig& cfg)
{
    if (m_mqttSubscriber)
    {
        m_mqttSubscriber->stop();
        m_mqttSubscriber.reset();
    }

    if (!cfg.enabled)
        return;

    m_mqttSubscriber =
        std::make_unique<nx::vms_server_plugins::analytics::stub::common::MqttSubscriber>(
            cfg.host, cfg.port, m_topic, cfg.username, cfg.password);
    m_mqttSubscriber->start();
}

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, ini().enableOutput)
{
    // Subscribe to per-camera detections: vms/ai/detections/<CAMERA_ID>
    std::string deviceId = deviceInfo && deviceInfo->id() ? deviceInfo->id() : "";
    if (!deviceId.empty() && deviceId.front() == '{')
        deviceId = deviceId.substr(1, deviceId.size() - 2);

    m_deviceId = deviceId;
    m_topic = "vms/ai/detections/" + deviceId;

    // Initial MQTT config from shared store (module `mqtt/` can override it via UI).
    m_mqttCfgVersion = nx::vms_server_plugins::analytics::stub::common::MqttConfigStore::instance().version();
    m_mqttCfg = nx::vms_server_plugins::analytics::stub::common::MqttConfigStore::instance().get();
    restartMqttSubscriber(m_mqttCfg);
}

DeviceAgent::~DeviceAgent()
{
    if (m_mqttSubscriber)
        m_mqttSubscriber->stop();
}

std::string DeviceAgent::manifestString() const
{
    return kDeviceAgentManifest;
}

bool DeviceAgent::pushCompressedVideoFrame(Ptr<const ICompressedVideoPacket> videoFrame)
{
    // Restart subscriber if MQTT settings were changed in the separate `mqtt` module.
    {
        const auto& store = nx::vms_server_plugins::analytics::stub::common::MqttConfigStore::instance();
        const uint64_t v = store.version();
        if (v != m_mqttCfgVersion)
        {
            m_mqttCfgVersion = v;
            m_mqttCfg = store.get();
            restartMqttSubscriber(m_mqttCfg);
        }
    }

    ++m_frameIndex;
    if ((m_frameIndex % kTrackLength) == 0)
        m_trackIds.clear();

    Ptr<IMetadataPacket> objectMetadataPacket = generateObjectMetadataPacket(
        videoFrame->timestampUs() + m_timestampShiftMs * 1000);

    if (objectMetadataPacket)
        pushMetadataPacket(objectMetadataPacket);

    return true;
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
            m_objectTypeIdsToGenerate.insert(key.substr(kObjectTypeGenerationSettingPrefix.size()));
        else if (key == kSendAttributesSetting)
            m_sendAttributes = toBool(value);
        else if (key == kTimeShiftSetting)
            m_timestampShiftMs = std::stoi(value);
    }

    return nullptr;
}

Uuid DeviceAgent::trackIdByTrackType(const std::string& typeId)
{
    auto& value = m_trackIds[typeId];
    if (value.isNull())
        value = UuidHelper::randomUuid();
    return value;
}

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
