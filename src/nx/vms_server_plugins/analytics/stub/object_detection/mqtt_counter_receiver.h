// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <mqtt/async_client.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

/**
 * MQTT receiver specifically for people counter (totalCount)
 * Uses a separate topic from detections
 */
class MqttCounterReceiver
{
public:
    MqttCounterReceiver(
        const std::string& broker,
        int port,
        const std::string& topic);
    
    ~MqttCounterReceiver();

    void start();
    void stop();
    
    /**
     * Get current total count (số người vào) from last MQTT message
     * @return total count value, or -1 if not available
     */
    int getTotalCount() const;
    
    /**
     * Check if we ever received any MQTT message
     * @return true if at least one message was received
     */
    bool hasReceivedData() const;

private:
    class Callback : public virtual mqtt::callback
    {
    public:
        explicit Callback(MqttCounterReceiver* receiver) : m_receiver(receiver) {}
        
        void connection_lost(const std::string& cause) override;
        void message_arrived(mqtt::const_message_ptr msg) override;
        
    private:
        MqttCounterReceiver* m_receiver;
    };
    
    void parseCounterMessage(const std::string& message);
    void reconnect();

private:
    std::string m_broker;
    int m_port;
    std::string m_topic;
    
    std::mutex m_mutex;
    std::atomic<int> m_totalCount{-1}; // Total count (số người vào) from MQTT
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
