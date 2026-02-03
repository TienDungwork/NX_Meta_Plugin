// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <thread>
#include <memory>
#include <map>
#include <unordered_map>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/helpers/uuid_helper.h>

#include "engine.h"
#include "mqtt_object_receiver.h"
#include "mqtt_counter_receiver.h"

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
        const nx::sdk::analytics::ICompressedVideoPacket* videoFrame) override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    nx::sdk::Uuid trackIdByTrackIndex(int trackIndex);

    nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket> generateObjectMetadataPacket(
        int64_t frameTimestampUs);
    
    std::vector<nx::sdk::Ptr<nx::sdk::analytics::IObjectTrackBestShotPacket>> generateBestShots(
        int64_t frameTimestampUs);

private:
    mutable std::mutex m_mutex;

    int m_frameIndex = 0;
    int m_timestampShiftMs = 0;
    bool m_sendAttributes = true;
    std::unordered_map<int, nx::sdk::Uuid> m_trackIds;
    std::set<std::string> m_objectTypeIdsToGenerate;
    
    // Track IDs that need Best Shot generation (newly detected objects)
    // Map: trackId -> boundingBox
    std::map<nx::sdk::Uuid, nx::sdk::analytics::Rect> m_trackIdsNeedingBestShot;
    
    // Track IDs that already have Best Shot generated (to avoid duplicates)
    std::set<nx::sdk::Uuid> m_trackIdsWithBestShot;
    
    // MQTT receiver for AI detections (bbox)
    std::unique_ptr<MqttObjectReceiver> m_mqttReceiver;
    
    // MQTT receiver for people counter (totalCount) - separate topic
    std::unique_ptr<MqttCounterReceiver> m_mqttCounterReceiver;
    
    // Add counter object to metadata packet
    void addCounterObject(nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> metadataPacket, int64_t frameTimestampUs);
};

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
