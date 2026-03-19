// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mqtt_object_receiver.h"

#include <algorithm>

#include <nx/kit/json.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "[MQTT Object Receiver] "
#include <nx/kit/debug.h>

namespace nx::vms_server_plugins::analytics::stub::object_detection {

void MqttObjectReceiver::Callback::connection_lost(const std::string& cause)
{
    NX_PRINT << "Connection lost: " << cause;
    {
        std::lock_guard<std::mutex> lock(m_receiver->m_objectsMutex);
        m_receiver->m_detectedObjects.clear();
        m_receiver->m_hasReceivedData.store(false);
    }
    m_receiver->reconnect();
}

void MqttObjectReceiver::Callback::message_arrived(mqtt::const_message_ptr msg)
{
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
    std::string serverAddress = "tcp://" + m_broker + ":" + std::to_string(m_port);
    std::string clientId = "vms_ai_receiver_" + m_topic;
    std::replace(clientId.begin(), clientId.end(), '/', '_');

    m_client = std::make_shared<mqtt::async_client>(serverAddress, clientId);
    m_callback = std::make_shared<Callback>(this);
    m_client->set_callback(*m_callback);

    m_connOpts.set_keep_alive_interval(20);
    m_connOpts.set_clean_session(true);
    m_connOpts.set_automatic_reconnect(true);

    // NOTE: Credentials are hardcoded to match your scripts.
    m_connOpts.set_user_name("atin");
    m_connOpts.set_password("team1@123#");
}

MqttObjectReceiver::~MqttObjectReceiver()
{
    stop();
}

void MqttObjectReceiver::start()
{
    try
    {
        m_client->connect(m_connOpts)->wait();
        m_client->subscribe(m_topic, 0)->wait();
        NX_PRINT << "Subscribed: " << m_topic;
    }
    catch (const mqtt::exception& exc)
    {
        NX_PRINT << "Start error: " << exc.what();
    }
}

void MqttObjectReceiver::stop()
{
    try
    {
        if (m_client && m_client->is_connected())
            m_client->disconnect()->wait();
    }
    catch (const mqtt::exception& exc)
    {
        NX_PRINT << "Stop error: " << exc.what();
    }
}

void MqttObjectReceiver::reconnect()
{
    try
    {
        m_client->connect(m_connOpts)->wait();
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
    std::string parseError;
    const nx::kit::Json data = nx::kit::Json::parse(message, parseError);
    if (!parseError.empty() || !data.is_object())
        return;

    if (!data["detections"].is_array())
        return;

    std::vector<DetectedObject> newObjects;
    for (const auto& detection : data["detections"].array_items())
    {
        if (!detection.is_object())
            continue;

        DetectedObject o;
        if (detection["label"].is_string())
            o.label = detection["label"].string_value();
        if (detection["confidence"].is_number())
            o.confidence = (float) detection["confidence"].number_value();
        if (detection["trackId"].is_number())
            o.trackId = detection["trackId"].int_value();

        if (detection["bbox"].is_array())
        {
            const auto bbox = detection["bbox"].array_items();
            if (bbox.size() >= 4)
            {
                o.x = (float) bbox[0].number_value();
                o.y = (float) bbox[1].number_value();
                o.width = (float) bbox[2].number_value();
                o.height = (float) bbox[3].number_value();
            }
        }

        newObjects.push_back(o);
    }

    {
        std::lock_guard<std::mutex> lock(m_objectsMutex);
        m_detectedObjects = std::move(newObjects);
        m_hasReceivedData.store(true);
    }
}

} // namespace nx::vms_server_plugins::analytics::stub::object_detection

