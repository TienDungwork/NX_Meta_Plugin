// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mqtt_counter_receiver.h"

#include <iostream>
#include <chrono>

#include <nx/kit/json.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "[MQTT Counter Receiver] "
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

MqttCounterReceiver::MqttCounterReceiver(
    const std::string& broker,
    int port,
    const std::string& topic)
    : m_broker(broker)
    , m_port(port)
    , m_topic(topic)
{
    m_client = std::make_shared<mqtt::async_client>(broker + ":" + std::to_string(port), "vms_counter_receiver_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    
    m_connOpts.set_clean_session(true);
    m_connOpts.set_automatic_reconnect(true);
    m_connOpts.set_keep_alive_interval(20);
    m_connOpts.set_connect_timeout(10);

    m_callback = std::make_shared<Callback>(this);
    m_client->set_callback(*m_callback);
    
    NX_PRINT << "MQTT Counter Receiver created (broker: " << m_broker 
             << ":" << m_port << ", topic: " << m_topic << ")";
}

MqttCounterReceiver::~MqttCounterReceiver()
{
    stop();
    NX_PRINT << "MQTT Counter Receiver destroyed";
}

void MqttCounterReceiver::start()
{
    try
    {
        if (m_client->is_connected())
        {
            NX_PRINT << "Already connected";
            return;
        }
        
        NX_PRINT << "Connecting to " << m_broker << ":" << m_port << "...";
        auto tok = m_client->connect(m_connOpts);
        tok->wait();
        
        NX_PRINT << "Connected, subscribing to " << m_topic;
        m_client->subscribe(m_topic, 0)->wait();
        
        NX_PRINT << "Subscribed successfully";
    }
    catch (const mqtt::exception& exc)
    {
        NX_PRINT << "Error: " << exc.what();
    }
}

void MqttCounterReceiver::stop()
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

void MqttCounterReceiver::reconnect()
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

int MqttCounterReceiver::getTotalCount() const
{
    return m_totalCount.load();
}

bool MqttCounterReceiver::hasReceivedData() const
{
    return m_hasReceivedData.load();
}

void MqttCounterReceiver::Callback::connection_lost(const std::string& cause)
{
    NX_PRINT << "Connection lost: " << cause;
    m_receiver->reconnect();
}

void MqttCounterReceiver::Callback::message_arrived(mqtt::const_message_ptr msg)
{
    NX_PRINT << "Message arrived on topic: " << msg->get_topic();
    m_receiver->parseCounterMessage(msg->get_payload_str());
}

void MqttCounterReceiver::parseCounterMessage(const std::string& message)
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
        
        // Parse totalCount
        if (obj.count("totalCount") > 0 && obj["totalCount"].is_number())
        {
            int totalCount = obj["totalCount"].int_value();
            m_totalCount.store(totalCount);
            m_hasReceivedData.store(true);
            NX_PRINT << "Total count (số người vào): " << totalCount;
        }
        else
        {
            NX_PRINT << "No 'totalCount' field found in message";
        }
    }
    catch (const std::exception& e)
    {
        NX_PRINT << "Exception in parseCounterMessage: " << e.what();
    }
}

} // namespace object_detection
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
