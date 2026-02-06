#include "mqtt_sub.h"

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <nx/kit/json.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "[MQTT Subscriber] "
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

MqttSubscriber::MqttSubscriber(
    const std::string& clientId,
    const std::string& broker,
    int port,
    const std::string& requestTopic,
    const std::string& responseTopic)
    : m_clientId(clientId)
    , m_broker(broker)
    , m_port(port)
    , m_requestTopic(requestTopic)
    , m_responseTopic(responseTopic)
{
    NX_PRINT << "MQTT Subscriber created (client: " << m_clientId
             << ", broker: " << m_broker 
             << ":" << m_port << ", request: " << m_requestTopic 
             << ", response: " << m_responseTopic << ")";
}

MqttSubscriber::~MqttSubscriber()
{
    stop();
    NX_PRINT << "MQTT Subscriber destroyed";
}

void MqttSubscriber::start()
{
    if (m_running.load())
    {
        NX_PRINT << "Already running";
        return;
    }

    m_running.store(true);
    m_thread = std::thread(&MqttSubscriber::workerThread, this);
    NX_PRINT << "MQTT Subscriber started";
}

void MqttSubscriber::stop()
{
    if (!m_running.load())
        return;

    m_running.store(false);

    if (m_socket >= 0)
    {
        close(m_socket);
        m_socket = -1;
    }

    if (m_thread.joinable())
        m_thread.join();

    NX_PRINT << "MQTT Subscriber stopped";
}

void MqttSubscriber::setRequestCallback(RequestCallback callback)
{
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_requestCallback = callback;
    NX_PRINT << "Request callback registered";
}

void MqttSubscriber::publishResponse(const std::string& message)
{
    try
    {
        // Tạo socket riêng cho response
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            NX_PRINT << "Failed to create response socket";
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

        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        {
            NX_PRINT << "Failed to connect for response";
            close(sock);
            return;
        }

        // CONNECT packet
        std::string connectPacket;
        connectPacket += (char)0x10;
        
        std::string clientId = "vms_roi_response";
        std::string protocolName = "MQTT";
        
        int connectPayloadLen = 2 + protocolName.length() + 1 + 1 + 2 + 2 + clientId.length();
        connectPacket += (char)connectPayloadLen;
        connectPacket += (char)0x00;
        connectPacket += (char)0x04;
        connectPacket += protocolName;
        connectPacket += (char)0x04;
        connectPacket += (char)0x02; // Clean session only, no auth
        connectPacket += (char)0x00;
        connectPacket += (char)0x3C;
        connectPacket += (char)((clientId.length() >> 8) & 0xFF);
        connectPacket += (char)(clientId.length() & 0xFF);
        connectPacket += clientId;
        
        send(sock, connectPacket.c_str(), connectPacket.length(), 0);
        
        char connackBuffer[4];
        ssize_t received = recv(sock, connackBuffer, 4, 0);
        
        if (received < 4 || connackBuffer[0] != 0x20)
        {
            NX_PRINT << "Failed to receive CONNACK for response";
            close(sock);
            return;
        }

        // PUBLISH packet
        std::string publishPacket;
        publishPacket += (char)0x30;
        
        int topicLen = m_responseTopic.length();
        int payloadLen = message.length();
        int remainingLength = 2 + topicLen + payloadLen;
        
        if (remainingLength < 128)
        {
            publishPacket += (char)remainingLength;
        }
        else
        {
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
        
        publishPacket += (char)(topicLen >> 8);
        publishPacket += (char)(topicLen & 0xFF);
        publishPacket += m_responseTopic;
        publishPacket += message;

        ssize_t sent = send(sock, publishPacket.c_str(), publishPacket.length(), 0);
        
        if (sent > 0)
        {
            NX_PRINT << "Published response: " << message;
        }
        
        char disconnectPacket[2] = {(char)0xE0, 0x00};
        send(sock, disconnectPacket, 2, 0);
        
        close(sock);
    }
    catch (const std::exception& e)
    {
        NX_PRINT << "Exception in publishResponse: " << e.what();
    }
}

void MqttSubscriber::workerThread()
{
    NX_PRINT << "Worker thread started";

    while (m_running.load())
    {
        if (!connectAndSubscribe())
        {
            NX_PRINT << "Failed to connect/subscribe, retrying in 5s...";
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        // Main receive loop
        while (m_running.load())
        {
            char buffer[4096];
            ssize_t bytesReceived = recv(m_socket, buffer, sizeof(buffer), 0);
            
            if (bytesReceived <= 0)
            {
                NX_PRINT << "Connection lost or error, reconnecting...";
                if (m_socket >= 0)
                {
                    close(m_socket);
                    m_socket = -1;
                }
                break;
            }

            // Parse MQTT packet
            if (buffer[0] == 0x30) // PUBLISH packet
            {
                try
                {
                    int multiplier = 1;
                    int remainingLength = 0;
                    int pos = 1;
                    
                    do
                    {
                        remainingLength += (buffer[pos] & 127) * multiplier;
                        multiplier *= 128;
                    } while ((buffer[pos++] & 128) != 0);

                    // Topic length
                    int topicLen = (buffer[pos] << 8) | buffer[pos + 1];
                    pos += 2;
                    
                    // Topic (skip)
                    pos += topicLen;
                    
                    // Payload
                    int payloadLen = remainingLength - 2 - topicLen;
                    if (payloadLen > 0 && pos + payloadLen <= bytesReceived)
                    {
                        std::string payload(buffer + pos, payloadLen);
                        handleIncomingMessage(payload);
                    }
                }
                catch (const std::exception& e)
                {
                    NX_PRINT << "Error parsing PUBLISH: " << e.what();
                }
            }
            else if (buffer[0] == 0xD0) // PINGRESP
            {
                NX_PRINT << "PINGRESP received";
            }
        }
    }

    if (m_socket >= 0)
    {
        close(m_socket);
        m_socket = -1;
    }

    NX_PRINT << "Worker thread stopped";
}

bool MqttSubscriber::connectAndSubscribe()
{
    // Create socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0)
    {
        NX_PRINT << "Failed to create socket";
        return false;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(m_port);
    
    if (inet_pton(AF_INET, m_broker.c_str(), &server.sin_addr) <= 0)
    {
        NX_PRINT << "Invalid broker address: " << m_broker;
        close(m_socket);
        m_socket = -1;
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (connect(m_socket, (struct sockaddr*)&server, sizeof(server)) < 0)
    {
        NX_PRINT << "Failed to connect to MQTT broker at " << m_broker << ":" << m_port;
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Send CONNECT packet
    std::string connectPacket;
    connectPacket += (char)0x10;
    
    std::string protocolName = "MQTT";
    
    int connectPayloadLen = 2 + protocolName.length() + 1 + 1 + 2 + 2 + m_clientId.length();
    connectPacket += (char)connectPayloadLen;
    connectPacket += (char)0x00;
    connectPacket += (char)0x04;
    connectPacket += protocolName;
    connectPacket += (char)0x04; // Protocol level
    connectPacket += (char)0x02; // Clean session only, no auth
    connectPacket += (char)0x00; // Keep alive MSB
    connectPacket += (char)0x3C; // Keep alive LSB (60s)
    connectPacket += (char)((m_clientId.length() >> 8) & 0xFF);
    connectPacket += (char)(m_clientId.length() & 0xFF);
    connectPacket += m_clientId;
    
    send(m_socket, connectPacket.c_str(), connectPacket.length(), 0);
    
    // Wait for CONNACK
    char connackBuffer[10];
    ssize_t received = recv(m_socket, connackBuffer, sizeof(connackBuffer), 0);
    
    if (received < 4 || connackBuffer[0] != 0x20)
    {
        NX_PRINT << "Failed to receive CONNACK (received " << received << " bytes)";
        if (received > 0)
        {
            NX_PRINT << "First byte: 0x" << std::hex << (int)(unsigned char)connackBuffer[0] << std::dec;
        }
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    NX_PRINT << "Connected to MQTT broker";

    // Send SUBSCRIBE packet (QoS 0)
    std::string subscribePacket;
    subscribePacket += (char)0x82; // SUBSCRIBE packet type
    
    int topicLen = m_requestTopic.length();
    int remainingLength = 2 + 2 + topicLen + 1; // packet ID + topic length + topic + QoS
    
    // Encode remaining length
    if (remainingLength < 128)
    {
        subscribePacket += (char)remainingLength;
    }
    else
    {
        int x = remainingLength;
        do
        {
            char encodedByte = x % 128;
            x = x / 128;
            if (x > 0)
                encodedByte |= 128;
            subscribePacket += encodedByte;
        } while (x > 0);
    }
    
    // Packet ID
    subscribePacket += (char)0x00;
    subscribePacket += (char)0x01;
    
    // Topic length (MSB, LSB)
    subscribePacket += (char)(topicLen >> 8);
    subscribePacket += (char)(topicLen & 0xFF);
    
    // Topic string
    subscribePacket += m_requestTopic;
    
    // QoS level for this subscription
    subscribePacket += (char)0x00; // QoS 0
    
    NX_PRINT << "Sending SUBSCRIBE for topic: " << m_requestTopic;
    ssize_t sent = send(m_socket, subscribePacket.c_str(), subscribePacket.length(), 0);
    
    if (sent <= 0)
    {
        NX_PRINT << "Failed to send SUBSCRIBE packet";
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    NX_PRINT << "SUBSCRIBE packet sent (" << sent << " bytes), waiting for SUBACK...";
    
    // Wait for SUBACK
    char subackBuffer[10];
    memset(subackBuffer, 0, sizeof(subackBuffer));
    received = recv(m_socket, subackBuffer, sizeof(subackBuffer), 0);
    
    NX_PRINT << "Received " << received << " bytes from broker";
    if (received > 0)
    {
        NX_PRINT << "First 4 bytes: 0x" << std::hex 
                 << (int)(unsigned char)subackBuffer[0] << " 0x"
                 << (int)(unsigned char)subackBuffer[1] << " 0x"
                 << (int)(unsigned char)subackBuffer[2] << " 0x"
                 << (int)(unsigned char)subackBuffer[3] << std::dec;
    }
    
    if (received < 3)
    {
        NX_PRINT << "Failed to receive SUBACK (received " << received << " bytes)";
        close(m_socket);
        m_socket = -1;
        return false;
    }
    
    // SUBACK packet format: [0x90] [remaining_length] [packet_id_msb] [packet_id_lsb] [return_code]
    unsigned char packetType = (unsigned char)subackBuffer[0];
    NX_PRINT << "Packet type check: 0x" << std::hex << (int)packetType << std::dec;
    
    if (packetType == 0x90)
    {
        // Success - check return code
        int returnCode = (unsigned char)subackBuffer[received - 1];
        if (returnCode == 0x80)
        {
            NX_PRINT << "SUBSCRIBE failed - broker rejected subscription";
            close(m_socket);
            m_socket = -1;
            return false;
        }
        
        NX_PRINT << "Subscribed successfully to topic: " << m_requestTopic 
                 << " (return code: 0x" << std::hex << returnCode << std::dec << ")";
        return true;
    }
    else
    {
        NX_PRINT << "Unexpected packet type: 0x" << std::hex << (int)packetType << std::dec;
        NX_PRINT << "Expected SUBACK (0x90), might be leftover from previous message";
        
        // Try to recv again to get SUBACK
        memset(subackBuffer, 0, sizeof(subackBuffer));
        received = recv(m_socket, subackBuffer, sizeof(subackBuffer), 0);
        if (received > 0 && (unsigned char)subackBuffer[0] == 0x90)
        {
            NX_PRINT << "Got SUBACK on second attempt";
            return true;
        }
        
        close(m_socket);
        m_socket = -1;
        return false;
    }
}

void MqttSubscriber::handleIncomingMessage(const std::string& message)
{
    NX_PRINT << "Received message: " << message;

    try
    {
        std::string parseError;
        nx::kit::Json jsonMsg = nx::kit::Json::parse(message, parseError);
        
        if (!parseError.empty() || !jsonMsg.is_object())
        {
            NX_PRINT << "Invalid JSON: " << parseError;
            return;
        }

        auto obj = jsonMsg.object_items();
        
        if (obj.count("action") == 0 || obj.count("camera_id") == 0)
        {
            NX_PRINT << "Missing 'action' or 'camera_id' field";
            return;
        }

        std::string action = obj["action"].string_value();
        std::string cameraId = obj["camera_id"].string_value();

        NX_PRINT << "Request: action=" << action << ", camera_id=" << cameraId;

        // Call callback
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        if (m_requestCallback)
        {
            m_requestCallback(cameraId, action);
        }
        else
        {
            NX_PRINT << "No callback registered!";
        }
    }
    catch (const std::exception& e)
    {
        NX_PRINT << "Exception in handleIncomingMessage: " << e.what();
    }
}

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
