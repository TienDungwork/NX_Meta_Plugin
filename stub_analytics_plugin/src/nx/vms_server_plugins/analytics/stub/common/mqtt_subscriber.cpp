// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mqtt_subscriber.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <functional>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "[MQTT Subscriber] "
#include <nx/kit/debug.h>

namespace nx::vms_server_plugins::analytics::stub::common {

namespace {

static std::string makeStableClientId(const std::string& topic)
{
    // MQTT broker drops old connections when clientId collides.
    // Use per-topic stable clientId so each camera subscriber keeps its own session.
    const auto h = (std::uint64_t) std::hash<std::string>{}(topic);
    char suffix[17]{};
    std::snprintf(suffix, sizeof(suffix), "%016llx", (unsigned long long) h);
    return std::string("nx_sub_") + suffix; // 7 + 16 = 23 chars (MQTT 3.1.1 friendly)
}

static bool sendAll(int fd, const void* data, size_t size)
{
    const uint8_t* p = (const uint8_t*) data;
    size_t left = size;
    while (left > 0)
    {
        const ssize_t n = ::send(fd, p, left, MSG_NOSIGNAL);
        if (n <= 0)
            return false;
        p += (size_t) n;
        left -= (size_t) n;
    }
    return true;
}

static bool recvAll(int fd, void* data, size_t size)
{
    uint8_t* p = (uint8_t*) data;
    size_t left = size;
    while (left > 0)
    {
        const ssize_t n = ::recv(fd, p, left, 0);
        if (n <= 0)
            return false;
        p += (size_t) n;
        left -= (size_t) n;
    }
    return true;
}

static void appendU16(std::string* out, uint16_t v)
{
    out->push_back((char) ((v >> 8) & 0xFF));
    out->push_back((char) (v & 0xFF));
}

static void appendBytes(std::string* out, const std::string& s)
{
    out->append(s);
}

static void appendString(std::string* out, const std::string& s)
{
    appendU16(out, (uint16_t) s.size());
    appendBytes(out, s);
}

static void appendRemainingLength(std::string* out, int len)
{
    // MQTT variable-length encoding.
    int x = len;
    do
    {
        uint8_t encoded = (uint8_t)(x % 128);
        x /= 128;
        if (x > 0)
            encoded |= 128;
        out->push_back((char) encoded);
    }
    while (x > 0);
}

static int readRemainingLength(int fd, uint32_t* outLen)
{
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t encoded = 0;
    int bytes = 0;
    do
    {
        if (!recvAll(fd, &encoded, 1))
            return -1;
        ++bytes;
        value += (encoded & 127u) * multiplier;
        multiplier *= 128u;
        if (multiplier > 128u * 128u * 128u * 128u)
            return -1;
    }
    while ((encoded & 128u) != 0);

    *outLen = value;
    return bytes;
}

static int connectTcp(const std::string& host, int port)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0)
        return -1;

    int fd = -1;
    for (addrinfo* p = res; p; p = p->ai_next)
    {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0)
            continue;
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0)
            break;
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(res);
    return fd;
}

static bool mqttConnect(
    int fd,
    const std::string& clientId,
    const std::string& username,
    const std::string& password)
{
    std::string vh; // variable header + payload

    // Protocol name + level
    appendString(&vh, "MQTT");
    vh.push_back((char) 0x04); // MQTT 3.1.1

    // Connect flags
    uint8_t flags = 0x02; // clean session
    if (!username.empty())
        flags |= 0x80;
    if (!password.empty())
        flags |= 0x40;
    vh.push_back((char) flags);

    // Keep alive = 20s
    appendU16(&vh, 20);

    // Payload: ClientId + [username] + [password]
    appendString(&vh, clientId);
    if (!username.empty())
        appendString(&vh, username);
    if (!password.empty())
        appendString(&vh, password);

    std::string pkt;
    pkt.push_back((char) 0x10); // CONNECT
    appendRemainingLength(&pkt, (int) vh.size());
    pkt += vh;

    if (!sendAll(fd, pkt.data(), pkt.size()))
        return false;

    // CONNACK: 0x20 0x02 0x00 <rc>
    uint8_t hdr[2]{};
    if (!recvAll(fd, hdr, 2))
        return false;
    if (hdr[0] != 0x20 || hdr[1] != 0x02)
        return false;
    uint8_t body[2]{};
    if (!recvAll(fd, body, 2))
        return false;
    const uint8_t rc = body[1];
    return rc == 0;
}

static bool mqttSubscribe(int fd, uint16_t packetId, const std::string& topic)
{
    std::string payload;
    appendString(&payload, topic);
    payload.push_back((char) 0x00); // QoS0

    std::string vh;
    appendU16(&vh, packetId);
    vh += payload;

    std::string pkt;
    pkt.push_back((char) 0x82); // SUBSCRIBE (QoS1)
    appendRemainingLength(&pkt, (int) vh.size());
    pkt += vh;

    if (!sendAll(fd, pkt.data(), pkt.size()))
        return false;

    // SUBACK: 0x90 <len> <packetId MSB> <packetId LSB> <returnCode...>
    uint8_t fixed{};
    if (!recvAll(fd, &fixed, 1))
        return false;
    if (fixed != 0x90)
        return false;
    uint32_t remLen = 0;
    if (readRemainingLength(fd, &remLen) < 0)
        return false;
    std::string rem;
    rem.resize(remLen);
    if (!recvAll(fd, rem.data(), remLen))
        return false;
    if (remLen < 3)
        return false;
    const uint16_t gotPid = (uint16_t)(((uint8_t)rem[0] << 8) | (uint8_t)rem[1]);
    if (gotPid != packetId)
        return false;
    const uint8_t rc = (uint8_t) rem[2];
    return rc <= 2; // 0=QoS0, 1=QoS1, 2=QoS2
}

static bool mqttReadOnePacket(int fd, uint8_t* outType, std::string* outPayload)
{
    uint8_t fixed{};
    if (!recvAll(fd, &fixed, 1))
        return false;

    uint32_t remLen = 0;
    if (readRemainingLength(fd, &remLen) < 0)
        return false;

    outPayload->resize(remLen);
    if (remLen > 0 && !recvAll(fd, outPayload->data(), remLen))
        return false;

    *outType = fixed;
    return true;
}

static bool mqttExtractPublishPayloadAndTopic(
    const uint8_t fixed,
    const std::string& rem,
    std::string* outTopic,
    std::string* outPayload)
{
    const uint8_t type = fixed & 0xF0;
    if (type != 0x30 && type != 0x31 && type != 0x32 && type != 0x33) // PUBLISH variants
        return false;
    if (rem.size() < 2)
        return false;
    const uint16_t topicLen = (uint16_t)(((uint8_t)rem[0] << 8) | (uint8_t)rem[1]);
    if (2 + (size_t) topicLen > rem.size())
        return false;
    *outTopic = rem.substr(2, topicLen);
    size_t pos = 2 + topicLen;
    if (pos > rem.size())
        return false;
    const uint8_t qos = (fixed >> 1) & 0x03;
    if (qos > 0)
    {
        // Packet ID present for QoS1/2
        if (pos + 2 > rem.size())
            return false;
        pos += 2;
    }
    *outPayload = rem.substr(pos);
    return true;
}

} // namespace

MqttSubscriber::MqttSubscriber(
    std::string host,
    int port,
    std::string topic,
    std::string username,
    std::string password)
    : m_host(std::move(host))
    , m_port(port)
    , m_topic(std::move(topic))
    , m_username(std::move(username))
    , m_password(std::move(password))
{
}

MqttSubscriber::~MqttSubscriber()
{
    stop();
}

void MqttSubscriber::start()
{
    if (m_running.exchange(true))
        return;
    m_stopRequested.store(false);
    m_thread = std::thread([this]{ threadMain(); });
}

void MqttSubscriber::stop()
{
    if (!m_running.exchange(false))
        return;
    m_stopRequested.store(true);
    if (m_thread.joinable())
        m_thread.join();
}

std::string MqttSubscriber::takeLastPayload()
{
    std::lock_guard<std::mutex> lock(m_payloadMutex);
    std::string r = std::move(m_lastPayload);
    m_lastPayload.clear();
    return r;
}

void MqttSubscriber::threadMain()
{
    const std::string clientId = makeStableClientId(m_topic);
    uint16_t pid = 1;

    while (!m_stopRequested.load())
    {
        const int fd = connectTcp(m_host, m_port);
        if (fd < 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        // Small timeouts so stop() doesn't hang forever.
        timeval tv{};
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (!mqttConnect(fd, clientId, m_username, m_password))
        {
            ::close(fd);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        if (!mqttSubscribe(fd, pid++, m_topic))
        {
            ::close(fd);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }

        NX_PRINT << "Subscribed " << m_topic << " on " << m_host << ":" << m_port;

        while (!m_stopRequested.load())
        {
            uint8_t fixed = 0;
            std::string rem;
            if (!mqttReadOnePacket(fd, &fixed, &rem))
            {
                // timeout or disconnect -> reconnect
                break;
            }

            std::string payload;
            std::string topic;
            if (mqttExtractPublishPayloadAndTopic(fixed, rem, &topic, &payload))
            {
                // Defensive filter: only accept detections from the exact per-camera topic.
                if (topic != m_topic)
                    continue;
                m_receivedPublishCount.fetch_add(1, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(m_payloadMutex);
                m_lastPayload = std::move(payload);
            }
        }

        ::close(fd);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace nx::vms_server_plugins::analytics::stub::common

