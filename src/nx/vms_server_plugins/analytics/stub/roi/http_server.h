#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <map>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

class HttpServer
{
public:
    using RequestCallback = std::function<std::string(const std::string& cameraId)>;

    HttpServer(int port = 8090);
    ~HttpServer();

    void start();
    void stop();
    
    void setRequestCallback(RequestCallback callback);

private:
    void serverThread();
    void handleClient(int clientSocket);
    std::string parseGetRequest(const std::string& request);
    std::string createHttpResponse(const std::string& body, int statusCode = 200);

private:
    int m_port;
    int m_serverSocket;
    std::atomic<bool> m_running;
    std::thread m_thread;
    RequestCallback m_requestCallback;
};

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
