// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mqtt_publisher.h"

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "[MQTT Publisher] "
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

MqttPublisher::MqttPublisher(
    const std::string& broker,
    int port,
    const std::string& topic)
    : m_broker(broker)
    , m_port(port)
    , m_topic(topic)
{
    NX_PRINT << "MQTT Publisher created (broker: " << m_broker 
             << ":" << m_port << ", topic: " << m_topic << ")";
}

MqttPublisher::~MqttPublisher()
{
    stop();
    NX_PRINT << "MQTT Publisher destroyed";
}

void MqttPublisher::start()
{
    if (m_running.load())
    {
        NX_PRINT << "Already running";
        return;
    }

    m_running.store(true);
    m_thread = std::thread(&MqttPublisher::workerThread, this);
    NX_PRINT << "MQTT Publisher started";
}

void MqttPublisher::stop()
{
    if (!m_running.load())
        return;

    m_running.store(false);
    m_queueCondition.notify_all();

    if (m_thread.joinable())
        m_thread.join();

    NX_PRINT << "MQTT Publisher stopped";
}

void MqttPublisher::publishPolygon(const std::string& polygonJson)
{
    if (!m_running.load())
    {
        NX_PRINT << "Publisher not running, starting automatically...";
        start();
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_messageQueue.push(polygonJson);
    }
    m_queueCondition.notify_one();

    NX_PRINT << "Queued polygon data for publishing";
}

void MqttPublisher::workerThread()
{
    NX_PRINT << "Worker thread started";

    while (m_running.load())
    {
        std::string message;
        
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this] {
                return !m_messageQueue.empty() || !m_running.load();
            });

            if (!m_running.load())
                break;

            if (!m_messageQueue.empty())
            {
                message = m_messageQueue.front();
                m_messageQueue.pop();
            }
        }

        if (!message.empty())
        {
            publishMessage(message);
        }
    }

    NX_PRINT << "Worker thread stopped";
}

bool MqttPublisher::connectToBroker()
{
    // TODO: Implement actual MQTT connection
    // Hiện tại dùng simple socket để demo
    return true;
}

void MqttPublisher::publishMessage(const std::string& message)
{
    try
    {
        // Simple TCP socket publish (raw MQTT packet)
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            NX_PRINT << "Failed to create socket";
            return;
        }

        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(m_port);
        
        if (inet_pton(AF_INET, m_broker.c_str(), &server.sin_addr) <= 0)
        {
            NX_PRINT << "Invalid broker address: " << m_broker;
            close(sock);
            return;
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        {
            NX_PRINT << "Failed to connect to MQTT broker at " << m_broker << ":" << m_port;
            close(sock);
            return;
        }

        // Step 1: Send MQTT CONNECT packet
        std::string connectPacket;
        connectPacket += (char)0x10; // CONNECT packet type
        
        std::string clientId = "vms_roi_plugin";
        std::string protocolName = "MQTT";
        
        // Calculate remaining length for CONNECT
        int connectPayloadLen = 2 + protocolName.length() + 1 + 1 + 2 + 2 + clientId.length();
        connectPacket += (char)connectPayloadLen;
        
        // Protocol name length + name
        connectPacket += (char)0x00;
        connectPacket += (char)0x04;
        connectPacket += protocolName;
        
        // Protocol level (MQTT 3.1.1 = 4)
        connectPacket += (char)0x04;
        
        // Connect flags (clean session)
        connectPacket += (char)0x02;
        
        // Keep alive (60 seconds)
        connectPacket += (char)0x00;
        connectPacket += (char)0x3C;
        
        // Client ID length + ID
        connectPacket += (char)((clientId.length() >> 8) & 0xFF);
        connectPacket += (char)(clientId.length() & 0xFF);
        connectPacket += clientId;
        
        // Send CONNECT
        send(sock, connectPacket.c_str(), connectPacket.length(), 0);
        
        // Wait for CONNACK
        char connackBuffer[4];
        ssize_t received = recv(sock, connackBuffer, 4, 0);
        
        if (received < 4 || connackBuffer[0] != 0x20)
        {
            NX_PRINT << "Failed to receive CONNACK from broker";
            close(sock);
            return;
        }
        
        NX_PRINT << "MQTT CONNACK received - connected to broker";

        // Step 2: Build and send MQTT PUBLISH packet (MQTT 3.1.1)
        std::string publishPacket;
        
        // Fixed header: PUBLISH (QoS 0)
        publishPacket += (char)0x30;
        
        // Remaining length calculation
        int topicLen = m_topic.length();
        int payloadLen = message.length();
        int remainingLength = 2 + topicLen + payloadLen;
        
        // Encode remaining length (simple single-byte for small messages)
        if (remainingLength < 128)
        {
            publishPacket += (char)remainingLength;
        }
        else
        {
            // Multi-byte encoding
            int x = remainingLength;
            do
            {
                char encodedByte = x % 128;
                x = x / 128;
                if (x > 0)
                    encodedByte |= 128;
                publishPacket += encodedByte;
            } while (x > 0);
        }
        
        // Variable header: Topic
        publishPacket += (char)(topicLen >> 8);
        publishPacket += (char)(topicLen & 0xFF);
        publishPacket += m_topic;
        
        // Payload
        publishPacket += message;

        // Send PUBLISH packet
        ssize_t sent = send(sock, publishPacket.c_str(), publishPacket.length(), 0);
        
        if (sent > 0)
        {
            NX_PRINT << "Published " << sent << " bytes to topic: " << m_topic;
            NX_PRINT << "Payload: " << message;
        }
        else
        {
            NX_PRINT << "Failed to send MQTT PUBLISH packet";
        }
        
        // Send DISCONNECT packet
        char disconnectPacket[2] = {(char)0xE0, 0x00};
        send(sock, disconnectPacket, 2, 0);
        
        close(sock);
    }
    catch (const std::exception& e)
    {
        NX_PRINT << "Exception in publishMessage: " << e.what();
    }
}

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
