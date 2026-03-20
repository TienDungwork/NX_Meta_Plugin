// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <cstdint>
#include <chrono>
#include <set>
#include <thread>
#include <vector>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "engine.h"
#include "../common/mqtt_subscriber.h"
#include "../common/mqtt_config_store.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    static const std::string kTimeShiftSetting;
    static const std::string kSendAttributesSetting;
    static const std::string kObjectTypeGenerationSettingPrefix;

public:
    DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual std::string manifestString() const override;

    virtual bool pushCompressedVideoFrame(
        nx::sdk::Ptr<const nx::sdk::analytics::ICompressedVideoPacket> videoFrame) override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    nx::sdk::Uuid trackIdByTrackType(const std::string& typeId);

    nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket> generateObjectMetadataPacket(
        int64_t frameTimestampUs);

    void restartMqttSubscriber(const nx::vms_server_plugins::analytics::stub::common::MqttConfig& cfg);

    /** Advance 1s windows and publish message count into m_mqttMsgsPerSecondDisplay. */
    void rollMqttMessageRateWindows(int64_t nowMs);

    /** HUD box + Name attribute so the Client shows msgs/s on screen. */
    void appendMqttMessageRateHud(const nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket>& packet);

private:
    mutable std::mutex m_mutex;

    int m_frameIndex = 0;
    int m_timestampShiftMs = 0;
    bool m_sendAttributes = true;
    std::map<std::string, nx::sdk::Uuid> m_trackIds;
    std::set<std::string> m_objectTypeIdsToGenerate;

    std::unique_ptr<nx::vms_server_plugins::analytics::stub::common::MqttSubscriber> m_mqttSubscriber;

    // Cached detections prepared from MQTT in a separate thread.
    // Frame thread should only addItem() from this cache (keep it fast).
    mutable std::mutex m_cacheMutex;
    std::vector<nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadata>> m_cachedObjects;

    // Milliseconds based on steady_clock::time_point (atomic for cheap reads).
    std::atomic<int64_t> m_lastMqttPayloadMs{0};

    // MQTT message rate: 1s windows vs MqttSubscriber::receivedPublishCount() (real PUBLISH count).
    int64_t m_mqttMsgRateWindowStartMs = 0;
    std::uint64_t m_mqttRxCountBaseline = 0;
    std::atomic<int> m_mqttMsgsPerSecondDisplay{0};

    std::atomic<bool> m_stopParseThread{false};
    std::thread m_parseThread;

    void mqttParseThreadMain();

    std::string m_deviceId;
    std::string m_topic;
    uint64_t m_mqttCfgVersion = 0;
    nx::vms_server_plugins::analytics::stub::common::MqttConfig m_mqttCfg;

    mutable std::mutex m_mqttSubscriberMutex;
};

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
