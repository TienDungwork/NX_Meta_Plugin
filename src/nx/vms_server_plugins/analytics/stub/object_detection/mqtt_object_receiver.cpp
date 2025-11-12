// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mqtt_object_receiver.h"

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>

#include <nx/kit/json.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "[MQTT Object Receiver] "
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_detection {

MqttObjectReceiver::MqttObjectReceiver(
    const std::string& broker,
    int port,
    const std::string& topic)
    : m_broker(broker)
    , m_port(port)
    , m_topic(topic)
{
    NX_PRINT << "Created (broker: " << m_broker << ":" << m_port << ", topic: " << m_topic << ")";
}

MqttObjectReceiver::~MqttObjectReceiver()
{
    stop();
    NX_PRINT << "Destroyed";
}

void MqttObjectReceiver::start()
{
    if (m_running.load())
    {
        NX_PRINT << "Already running";
        return;
    }

    m_running.store(true);
    m_thread = std::thread(&MqttObjectReceiver::workerThread, this);
    NX_PRINT << "Started";
}

void MqttObjectReceiver::stop()
{
    if (!m_running.load())
        return;

    m_running.store(false);

    if (m_thread.joinable())
        m_thread.join();

    NX_PRINT << "Stopped";
}

std::vector<DetectedObject> MqttObjectReceiver::getDetectedObjects()
{
    std::lock_guard<std::mutex> lock(m_objectsMutex);
    return m_detectedObjects;
}

bool MqttObjectReceiver::hasReceivedData() const
{
    return m_hasReceivedData.load();
}

void MqttObjectReceiver::clearObjects()
{
    std::lock_guard<std::mutex> lock(m_objectsMutex);
    m_detectedObjects.clear();
}

void MqttObjectReceiver::workerThread()
{
    NX_PRINT << "Worker thread started";

    while (m_running.load())
    {
        if (connectToBroker())
        {
            receiveMessages();
        }
        
        if (m_running.load())
        {
            NX_PRINT << "Reconnecting in 5 seconds...";
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    NX_PRINT << "Worker thread stopped";
}

bool MqttObjectReceiver::connectToBroker()
{
    NX_PRINT << "Connecting to broker " << m_broker << ":" << m_port;
    
    // Create socket
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockfd < 0)
    {
        NX_PRINT << "Failed to create socket";
        return false;
    }

    // Set socket timeout to 30 seconds
    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    setsockopt(m_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    // Connect to broker
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_port);
    inet_pton(AF_INET, m_broker.c_str(), &serverAddr.sin_addr);

    if (connect(m_sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
    {
        NX_PRINT << "Failed to connect to broker";
        close(m_sockfd);
        m_sockfd = -1;
        return false;
    }

    NX_PRINT << "Socket connected, sending MQTT CONNECT";

    // Send MQTT CONNECT packet
    std::vector<uint8_t> connectPacket;
    connectPacket.push_back(0x10); // CONNECT message type
    
    std::string clientId = "vms_ai_receiver";
    std::string protocolName = "MQTT";
    
    // Calculate remaining length
    int remainingLength = 2 + protocolName.length() + 1 + 1 + 2 + 2 + clientId.length();
    connectPacket.push_back(remainingLength);
    
    // Protocol name length
    connectPacket.push_back(0x00);
    connectPacket.push_back(protocolName.length());
    for (char c : protocolName)
        connectPacket.push_back(c);
    
    // Protocol level (MQTT 3.1.1)
    connectPacket.push_back(0x04);
    
    // Connect flags (clean session)
    connectPacket.push_back(0x02);
    
    // Keep alive
    connectPacket.push_back(0x00);
    connectPacket.push_back(0x3C); // 60 seconds
    
    // Client ID length
    connectPacket.push_back((clientId.length() >> 8) & 0xFF);
    connectPacket.push_back(clientId.length() & 0xFF);
    for (char c : clientId)
        connectPacket.push_back(c);
    
    send(m_sockfd, connectPacket.data(), connectPacket.size(), 0);
    
    // Wait for CONNACK
    uint8_t connackBuffer[4];
    int bytesRead = recv(m_sockfd, connackBuffer, sizeof(connackBuffer), 0);
    if (bytesRead < 4 || connackBuffer[0] != 0x20)
    {
        NX_PRINT << "Failed to receive CONNACK";
        close(m_sockfd);
        m_sockfd = -1;
        return false;
    }
    
    NX_PRINT << "Received CONNACK, sending SUBSCRIBE to " << m_topic;
    
    // Send SUBSCRIBE packet
    std::vector<uint8_t> subscribePacket;
    subscribePacket.push_back(0x82); // SUBSCRIBE message type with QoS 1
    
    // Calculate remaining length
    int subRemainingLength = 2 + 2 + m_topic.length() + 1;
    subscribePacket.push_back(subRemainingLength);
    
    // Packet identifier
    subscribePacket.push_back(0x00);
    subscribePacket.push_back(0x01);
    
    // Topic length
    subscribePacket.push_back((m_topic.length() >> 8) & 0xFF);
    subscribePacket.push_back(m_topic.length() & 0xFF);
    for (char c : m_topic)
        subscribePacket.push_back(c);
    
    // QoS level
    subscribePacket.push_back(0x00);
    
    send(m_sockfd, subscribePacket.data(), subscribePacket.size(), 0);
    
    // Wait for SUBACK
    uint8_t subackBuffer[5];
    bytesRead = recv(m_sockfd, subackBuffer, sizeof(subackBuffer), 0);
    if (bytesRead < 5 || subackBuffer[0] != 0x90)
    {
        NX_PRINT << "Failed to receive SUBACK";
        close(m_sockfd);
        m_sockfd = -1;
        return false;
    }

    NX_PRINT << "Successfully subscribed to " << m_topic;
    return true;
}

void MqttObjectReceiver::receiveMessages()
{
    NX_PRINT << "Starting message receive loop";
    
    std::vector<uint8_t> buffer(4096);
    int messageCount = 0;
    
    while (m_running.load())
    {
        int n = recv(m_sockfd, buffer.data(), buffer.size(), 0);
        
        NX_PRINT << "recv() returned: " << n << " (errno: " << errno << ")";
        
        if (n <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Timeout, continue
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            NX_PRINT << "Connection lost (recv returned " << n << ", errno: " << errno << ")";
            break;
        }

        messageCount++;
        NX_PRINT << "Received data packet #" << messageCount << ", " << n << " bytes";
        NX_PRINT << "First byte: 0x" << std::hex << (int)buffer[0] << std::dec;

        // Parse MQTT PUBLISH message
        if ((buffer[0] & 0xF0) == 0x30) // PUBLISH
        {
            NX_PRINT << "This is a PUBLISH message";
            int pos = 1;
            
            // Read remaining length
            int multiplier = 1;
            int remainingLength = 0;
            uint8_t encodedByte;
            do
            {
                encodedByte = buffer[pos++];
                remainingLength += (encodedByte & 127) * multiplier;
                multiplier *= 128;
            } while ((encodedByte & 128) != 0);
            
            NX_PRINT << "Remaining length: " << remainingLength;
            
            // Read topic length
            int topicLength = (buffer[pos] << 8) | buffer[pos + 1];
            pos += 2;
            
            NX_PRINT << "Topic length: " << topicLength;
            
            // Read topic
            std::string topic(reinterpret_cast<char*>(&buffer[pos]), topicLength);
            pos += topicLength;
            
            NX_PRINT << "Topic: " << topic;
            
            // Read payload
            int payloadLength = remainingLength - 2 - topicLength;
            std::string payload(reinterpret_cast<char*>(&buffer[pos]), payloadLength);
            
            NX_PRINT << "Received MQTT message (" << payload.length() << " bytes)";
            NX_PRINT << "Payload: " << payload;
            
            // Parse and store detections
            parseDetectionMessage(payload);
        }
        else
        {
            NX_PRINT << "NOT a PUBLISH message (type: 0x" << std::hex << (int)(buffer[0] & 0xF0) << std::dec << ")";
        }
    }
    
    // Send DISCONNECT
    if (m_sockfd >= 0)
    {
        uint8_t disconnectPacket[] = {0xE0, 0x00};
        send(m_sockfd, disconnectPacket, sizeof(disconnectPacket), 0);
        close(m_sockfd);
        m_sockfd = -1;
    }
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
        
        // Expect format:
        // {
        //   "detections": [
        //     {
        //       "label": "person",
        //       "confidence": 0.95,
        //       "bbox": [x, y, width, height],  // normalized 0-1
        //       "trackId": 1
        //     }
        //   ]
        // }
        
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
        
        // Update detected objects with timestamp
        {
            std::lock_guard<std::mutex> lock(m_objectsMutex);
            m_detectedObjects = newObjects;
            
            // Update timestamp
            m_lastMessageTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
                
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
