#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

class MqttSubscriber
{
public:
    using RequestCallback = std::function<void(const std::string& cameraId, const std::string& action)>;

    MqttSubscriber(
        const std::string& clientId,
        const std::string& broker = "103.9.158.149",
        int port = 1883,
        const std::string& requestTopic = "vms/roi/request",
        const std::string& responseTopic = "vms/roi/response");
    
    ~MqttSubscriber();

    void start();
    void stop();
    void setRequestCallback(RequestCallback callback);
    void publishResponse(const std::string& message);

private:
    void workerThread();
    bool connectAndSubscribe();
    void handleIncomingMessage(const std::string& message);
    
    std::string m_clientId;
    std::string m_broker;
    int m_port;
    std::string m_requestTopic;
    std::string m_responseTopic;
    
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    
    RequestCallback m_requestCallback;
    std::mutex m_callbackMutex;
    
    int m_socket = -1;
};

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
