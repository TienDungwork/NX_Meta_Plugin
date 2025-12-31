// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mqtt_object_receiver.h"

#include <iostream>
#include <chrono>
#include <algorithm>

#include <nx/kit/json.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "[MQTT Object Receiver] "
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

void MqttObjectReceiver::Callback::connection_lost(const std::string& cause)
{
    NX_PRINT << "Connection lost: " << cause;
    
    // Clear objects and reset flag when connection is lost
    {
        std::lock_guard<std::mutex> lock(m_receiver->m_objectsMutex);
        m_receiver->m_detectedObjects.clear();
        m_receiver->m_hasReceivedData.store(false);
    }
    
    // Try to reconnect
    m_receiver->reconnect();
}

void MqttObjectReceiver::Callback::message_arrived(mqtt::const_message_ptr msg)
{
    NX_PRINT << "Message arrived on topic: " << msg->get_topic();
    NX_PRINT << "Payload (" << msg->get_payload().length() << " bytes): " << msg->get_payload_str();
    
    m_receiver->parseDetectionMessage(msg->get_payload_str());
}

MqttObjectReceiver::MqttObjectReceiver(
    const std::string& broker,
    int port,
    const std::string& topic)
    : m_broker(broker)
    , m_port(port)
    , m_topic(topic)
{
    NX_PRINT << "Created (broker: " << m_broker << ":" << m_port << ", topic: " << m_topic << ")";
    
    // Create MQTT client with unique ID based on topic (includes camera ID)
    std::string serverAddress = "tcp://" + m_broker + ":" + std::to_string(m_port);
    std::string clientId = "vms_ai_receiver_" + m_topic;
    // Replace slashes in topic to make valid client ID
    std::replace(clientId.begin(), clientId.end(), '/', '_');
    
    NX_PRINT << "MQTT Client ID: " << clientId;
    
    m_client = std::make_shared<mqtt::async_client>(serverAddress, clientId);
    m_callback = std::make_shared<Callback>(this);
    m_client->set_callback(*m_callback);
    
    // Configure connection options
    m_connOpts.set_keep_alive_interval(20);
    m_connOpts.set_clean_session(true);
    m_connOpts.set_automatic_reconnect(true);
}

MqttObjectReceiver::~MqttObjectReceiver()
{
    stop();
    NX_PRINT << "Destroyed";
}

void MqttObjectReceiver::start()
{
    NX_PRINT << "Starting connection...";
    
    try
    {
        auto tok = m_client->connect(m_connOpts);
        tok->wait();
        
        NX_PRINT << "Connected to broker, subscribing to " << m_topic;
        
        m_client->subscribe(m_topic, 0)->wait();
        
        NX_PRINT << "Successfully subscribed";
    }
    catch (const mqtt::exception& exc)
    {
        NX_PRINT << "Error: " << exc.what();
    }
}

void MqttObjectReceiver::stop()
{
    try
    {
        if (m_client && m_client->is_connected())
        {
            NX_PRINT << "Disconnecting...";
            m_client->disconnect()->wait();
        }
    }
    catch (const mqtt::exception& exc)
    {
        NX_PRINT << "Error during disconnect: " << exc.what();
    }
}

void MqttObjectReceiver::reconnect()
{
    NX_PRINT << "Attempting to reconnect...";
    
    try
    {
        auto tok = m_client->connect(m_connOpts);
        tok->wait();
        
        NX_PRINT << "Reconnected, resubscribing to " << m_topic;
        m_client->subscribe(m_topic, 0)->wait();
    }
    catch (const mqtt::exception& exc)
    {
        NX_PRINT << "Reconnect failed: " << exc.what();
    }
}

std::vector<DetectedObject> MqttObjectReceiver::getAndClearDetectedObjects()
{
    std::lock_guard<std::mutex> lock(m_objectsMutex);
    
    // Get objects and clear immediately (consume pattern)
    std::vector<DetectedObject> result = std::move(m_detectedObjects);
    m_detectedObjects.clear();
    
    return result;
}

bool MqttObjectReceiver::hasReceivedData() const
{
    return m_hasReceivedData.load();
}

void MqttObjectReceiver::parseDetectionMessage(const std::string& message)
{
    try
    {
        std::string parseError;
        nx::kit::Json data = nx::kit::Json::parse(message, parseError);
        
        if (!parseError.empty() || !data.is_object())
        {
            NX_PRINT << "Failed to parse JSON: " << parseError;
            return;
        }
        
        auto obj = data.object_items();
        
        if (obj.count("detections") == 0 || !obj["detections"].is_array())
        {
            NX_PRINT << "No 'detections' array found in message";
            return;
        }
        
        std::vector<DetectedObject> newObjects;
        
        for (const auto& detection : obj["detections"].array_items())
        {
            if (!detection.is_object())
                continue;
            
            auto detObj = detection.object_items();
            
            DetectedObject detected;
            
            if (detObj.count("label") > 0 && detObj["label"].is_string())
                detected.label = detObj["label"].string_value();
            
            if (detObj.count("confidence") > 0 && detObj["confidence"].is_number())
                detected.confidence = detObj["confidence"].number_value();
            
            if (detObj.count("trackId") > 0 && detObj["trackId"].is_number())
                detected.trackId = detObj["trackId"].int_value();
            
            if (detObj.count("name") > 0 && detObj["name"].is_string())
                detected.name = detObj["name"].string_value();
            
            if (detObj.count("bbox") > 0 && detObj["bbox"].is_array())
            {
                auto bbox = detObj["bbox"].array_items();
                if (bbox.size() >= 4)
                {
                    detected.x = bbox[0].number_value();
                    detected.y = bbox[1].number_value();
                    detected.width = bbox[2].number_value();
                    detected.height = bbox[3].number_value();
                    
                    newObjects.push_back(detected);
                }
            }
        }
        
        // Store objects in queue (will be consumed by next getAndClearDetectedObjects call)
        {
            std::lock_guard<std::mutex> lock(m_objectsMutex);
            m_detectedObjects = newObjects;
            
            // Mark that we've received at least one MQTT message
            m_hasReceivedData.store(true);
        }
        
        NX_PRINT << "Parsed " << newObjects.size() << " objects";
        for (const auto& obj : newObjects)
        {
            NX_PRINT << "  - " << obj.label << " @ [" << obj.x << "," << obj.y 
                     << "," << obj.width << "," << obj.height << "] conf=" << obj.confidence;
        }
    }
    catch (const std::exception& e)
    {
        NX_PRINT << "Exception in parseDetectionMessage: " << e.what();
    }
}

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
