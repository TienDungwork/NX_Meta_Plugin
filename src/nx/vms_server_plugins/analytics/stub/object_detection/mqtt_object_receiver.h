// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <mqtt/async_client.h>

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
     * Get and consume detected objects (thread-safe)
     * Returns objects from queue and clears the queue immediately
     * @return Current list of detected objects (will be empty after call)
     */
    std::vector<DetectedObject> getAndClearDetectedObjects();
    
    /**
     * Check if we ever received any MQTT message
     * @return true if at least one message was received
     */
    bool hasReceivedData() const;

private:
    class Callback : public virtual mqtt::callback
    {
    public:
        explicit Callback(MqttObjectReceiver* receiver) : m_receiver(receiver) {}
        
        void connection_lost(const std::string& cause) override;
        void message_arrived(mqtt::const_message_ptr msg) override;
        
    private:
        MqttObjectReceiver* m_receiver;
    };
    
    void parseDetectionMessage(const std::string& message);
    void reconnect();

private:
    std::string m_broker;
    int m_port;
    std::string m_topic;
    
    std::mutex m_objectsMutex;
    std::vector<DetectedObject> m_detectedObjects;  // Queue of objects to be consumed
    std::atomic<bool> m_hasReceivedData{false}; // Track if we've ever received MQTT data
    
    std::shared_ptr<mqtt::async_client> m_client;
    std::shared_ptr<Callback> m_callback;
    mqtt::connect_options m_connOpts;
};

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
