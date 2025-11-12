// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

class MqttPublisher
{
public:
    MqttPublisher(
        const std::string& broker = "192.168.1.215",
        int port = 1883,
        const std::string& topic = "vms/roi/polygon");
    
    ~MqttPublisher();

    /**
     * Publish polygon data (non-blocking)
     * @param polygonJson: JSON string chứa polygon data
     */
    void publishPolygon(const std::string& polygonJson);
    void start();
    void stop();

private:
    void workerThread();
    bool connectToBroker();
    void publishMessage(const std::string& message);

private:
    std::string m_broker;
    int m_port;
    std::string m_topic;
    
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::queue<std::string> m_messageQueue;
};

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
