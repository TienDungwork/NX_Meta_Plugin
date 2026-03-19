// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <mqtt/async_client.h>

namespace nx::vms_server_plugins::analytics::stub::object_detection {

struct DetectedObject
{
    std::string label;          //< class name
    float confidence = 1.0F;    //< 0.0 - 1.0
    float x = 0.0F;             //< normalized 0-1
    float y = 0.0F;             //< normalized 0-1
    float width = 0.0F;         //< normalized 0-1
    float height = 0.0F;        //< normalized 0-1
    int trackId = 0;            //< unique ID for tracking
};

class MqttObjectReceiver
{
public:
    MqttObjectReceiver(const std::string& broker, int port, const std::string& topic);
    ~MqttObjectReceiver();

    void start();
    void stop();

    /** Consume current objects and clear. */
    std::vector<DetectedObject> getAndClearDetectedObjects();
    bool hasReceivedData() const;

private:
    class Callback : public virtual mqtt::callback
    {
    public:
        explicit Callback(MqttObjectReceiver* receiver) : m_receiver(receiver) {}
        void connection_lost(const std::string& cause) override;
        void message_arrived(mqtt::const_message_ptr msg) override;
    private:
        MqttObjectReceiver* m_receiver = nullptr;
    };

    void parseDetectionMessage(const std::string& message);
    void reconnect();

private:
    std::string m_broker;
    int m_port = 1883;
    std::string m_topic;

    std::mutex m_objectsMutex;
    std::vector<DetectedObject> m_detectedObjects;
    std::atomic<bool> m_hasReceivedData{false};

    std::shared_ptr<mqtt::async_client> m_client;
    std::shared_ptr<Callback> m_callback;
    mqtt::connect_options m_connOpts;
};

} // namespace nx::vms_server_plugins::analytics::stub::object_detection

