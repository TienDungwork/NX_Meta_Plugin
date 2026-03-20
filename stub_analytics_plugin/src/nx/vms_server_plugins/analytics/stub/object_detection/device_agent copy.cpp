// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>
#include <cctype>
#include <nx/kit/json.h>
#include <nx/kit/debug.h>

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
static constexpr int kMqttHoldMs = 300; // Clear stale boxes quickly when MQTT stops.
const std::string DeviceAgent::kTimeShiftSetting = "timestampShiftMs";
const std::string DeviceAgent::kSendAttributesSetting = "sendAttributes";
const std::string DeviceAgent::kObjectTypeGenerationSettingPrefix = "objectTypeIdToGenerate.";

static std::string normalizeLabelToLower(const std::string& label)
{
    std::string r;
    r.reserve(label.size());
    for (unsigned char c : label)
        r.push_back((char) std::tolower(c));

    // Trim leading/trailing whitespace to make mapping more robust.
    const auto start = r.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return {};
    const auto end = r.find_last_not_of(" \t\r\n");
    return r.substr(start, end - start + 1);
}

static std::string objectTypeIdFromLabel(const std::string& label)
{
    const std::string l = normalizeLabelToLower(label);
    // Map incoming MQTT labels to supported taxonomy object types.
    // Supported only:
    // - Face:    "face", "person", "people"
    // - Vehicle: "vehicle", "car"
    if (l == "person" || l == "people" || l == "face")
        return "nx.base.Face";
    if (l == "vehicle" || l == "car")
        return "nx.base.Vehicle";
    return "nx.base.Unknown";
}

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
    const auto nowMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();

    // Clear stale boxes when MQTT stops, to avoid "stuck boxes".
    const int64_t lastMs = m_lastMqttPayloadMs.load(std::memory_order_relaxed);
    if (lastMs != 0 && (nowMs - lastMs) > kMqttHoldMs)
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        // Re-check after acquiring lock to avoid racing with parse thread updates.
        const int64_t lastMs2 = m_lastMqttPayloadMs.load(std::memory_order_relaxed);
        if (lastMs2 != 0 && (nowMs - lastMs2) > kMqttHoldMs)
            m_cachedObjects.clear();
    }

    std::vector<Ptr<ObjectMetadata>> cached;
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        cached = m_cachedObjects;
    }
    for (const auto& obj : cached)
        metadataPacket->addItem(obj);

    return metadataPacket;
}

void DeviceAgent::restartMqttSubscriber(
    const nx::vms_server_plugins::analytics::stub::common::MqttConfig& cfg)
{
    std::lock_guard<std::mutex> lock(m_mqttSubscriberMutex);

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
    if (!deviceId.empty() && deviceId.front() == '{' && deviceId.back() == '}' && deviceId.size() > 2)
        deviceId = deviceId.substr(1, deviceId.size() - 2);

    m_deviceId = deviceId;
    m_topic = "vms/ai/detections/" + deviceId;

    // Initial MQTT config from shared store (module `mqtt/` can override it via UI).
    m_mqttCfgVersion = nx::vms_server_plugins::analytics::stub::common::MqttConfigStore::instance().version();
    m_mqttCfg = nx::vms_server_plugins::analytics::stub::common::MqttConfigStore::instance().get();
    restartMqttSubscriber(m_mqttCfg);

    m_stopParseThread.store(false);
    m_parseThread = std::thread([this]{ mqttParseThreadMain(); });
}

DeviceAgent::~DeviceAgent()
{
    m_stopParseThread.store(true);
    if (m_parseThread.joinable())
        m_parseThread.join();

    std::lock_guard<std::mutex> lock(m_mqttSubscriberMutex);
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

    Ptr<IMetadataPacket> objectMetadataPacket = generateObjectMetadataPacket(
        videoFrame->timestampUs() + m_timestampShiftMs * 1000);
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

void DeviceAgent::mqttParseThreadMain()
{
    while (!m_stopParseThread.load(std::memory_order_relaxed))
    {
        std::string payload;
        {
            std::lock_guard<std::mutex> lock(m_mqttSubscriberMutex);
            if (m_mqttSubscriber)
                payload = m_mqttSubscriber->takeLastPayload();
        }

        if (payload.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const auto nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();

        std::string parseError;
        const nx::kit::Json data = nx::kit::Json::parse(payload, parseError);
        if (!parseError.empty() || !data.is_object() || !data["detections"].is_array())
            continue;

        bool sendAttributesLocal = false;
        std::set<std::string> objectTypeIdsToGenerateLocal;
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            sendAttributesLocal = m_sendAttributes;
            objectTypeIdsToGenerateLocal = m_objectTypeIdsToGenerate;
        }

        std::vector<Ptr<ObjectMetadata>> objectsAllowed;
        std::vector<Ptr<ObjectMetadata>> objectsAll;

        for (const auto& det : data["detections"].array_items())
        {
            if (!det.is_object())
                continue;

            const std::string label =
                det["label"].is_string() ? det["label"].string_value() : "";
            const std::string typeId = objectTypeIdFromLabel(label);
            if (typeId == "nx.base.Unknown")
                continue;

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
            obj->setTypeId(typeId);

            const std::string trackKey = typeId + "#" + std::to_string(trackId);
            obj->setBoundingBox(Rect{x, y, w, h});
            obj->setConfidence(confidence);
            obj->setTrackId(trackIdByTrackType(trackKey));

            const bool typeAllowed =
                objectTypeIdsToGenerateLocal.empty() ||
                objectTypeIdsToGenerateLocal.find(typeId)
                    != objectTypeIdsToGenerateLocal.cend();

            if (sendAttributesLocal && typeAllowed)
            {
                if (auto it = kObjectAttributes.find(typeId); it != kObjectAttributes.cend())
                {
                    for (const auto& attr : it->second)
                        obj->addAttribute(makePtr<Attribute>(attr.first, attr.second));
                }
            }

            objectsAll.push_back(obj);
            if (typeAllowed)
                objectsAllowed.push_back(obj);
        }

        if (objectsAllowed.empty() && !objectsAll.empty())
            objectsAllowed = std::move(objectsAll);

        m_lastMqttPayloadMs.store(nowMs, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_cachedObjects = std::move(objectsAllowed);
        }
    }
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
