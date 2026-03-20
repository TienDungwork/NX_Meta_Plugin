// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace nx::vms_server_plugins::analytics::stub::common {

/**
 * Minimal MQTT 3.1.1 subscriber (QoS0) over TCP sockets.
 * Supports: CONNECT (username/password), SUBSCRIBE, receiving PUBLISH payloads.
 *
 * This is intentionally tiny to avoid external MQTT C++ dependencies.
 */
class MqttSubscriber
{
public:
    MqttSubscriber(
        std::string host,
        int port,
        std::string topic,
        std::string username,
        std::string password);

    ~MqttSubscriber();

    void start();
    void stop();

    bool isRunning() const { return m_running.load(); }

    /** Returns last received payload (and clears it). Empty if none. */
    std::string takeLastPayload();

    /** Total PUBLISH payloads received on this connection (each MQTT message, not coalesced). */
    std::uint64_t receivedPublishCount() const
    {
        return m_receivedPublishCount.load(std::memory_order_relaxed);
    }

private:
    void threadMain();

private:
    std::string m_host;
    int m_port = 1883;
    std::string m_topic;
    std::string m_username;
    std::string m_password;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    std::thread m_thread;

    mutable std::mutex m_payloadMutex;
    std::string m_lastPayload;

    std::atomic<std::uint64_t> m_receivedPublishCount{0};
};

} // namespace nx::vms_server_plugins::analytics::stub::common

