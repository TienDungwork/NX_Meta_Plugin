// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

struct DetectedObject
{
    std::string label;          // class name
    float confidence;           // 0.0 - 1.0
    float x;                    // normalized 0-1
    float y;                    // normalized 0-1
    float width;                // normalized 0-1
    float height;               // normalized 0-1
    int trackId;                // unique ID for tracking
};

class MqttObjectReceiver
{
public:
    MqttObjectReceiver(
        const std::string& broker,
        int port,
        const std::string& topic);
    
    ~MqttObjectReceiver();

    void start();
    void stop();

    /**
     * Get detected objects (thread-safe)
     * Returns empty if data is too old (>2 seconds)
     * @return Current list of detected objects
     */
    std::vector<DetectedObject> getDetectedObjects();
    
    /**
     * Check if we ever received any MQTT message
     * @return true if at least one message was received
     */
    bool hasReceivedData() const;
    
    void clearObjects();

private:
    void workerThread();
    bool connectToBroker();
    void receiveMessages();
    void parseDetectionMessage(const std::string& message);

private:
    std::string m_broker;
    int m_port;
    std::string m_topic;
    
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    
    std::mutex m_objectsMutex;
    std::vector<DetectedObject> m_detectedObjects;
    int64_t m_lastMessageTimeUs = 0;  // Timestamp of last received message
    std::atomic<bool> m_hasReceivedData{false}; // Track if we've ever received MQTT data
    
    int m_sockfd = -1;
};

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
