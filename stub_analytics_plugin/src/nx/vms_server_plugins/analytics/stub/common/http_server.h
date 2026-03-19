// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace nx::vms_server_plugins::analytics::stub::common {

class HttpServer
{
public:
    struct Request
    {
        std::string method;   // "GET", "POST"
        std::string path;     // "/boxes"
        std::string body;     // raw body
        std::string query;    // part after '?', not parsed
    };

    struct Response
    {
        int statusCode = 200;
        std::string contentType = "application/json";
        std::string body = "{}";
    };

    using Handler = std::function<Response(const Request&)>;

    HttpServer();
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void setHandler(std::string method, std::string path, Handler handler);

    /** Starts listening on 0.0.0.0:port. Safe to call multiple times (restarts if port differs). */
    bool start(int port);

    void stop();

    bool isRunning() const { return m_running.load(); }
    int port() const { return m_port; }

private:
    void threadMain();
    void closeListenSocket();

    Response dispatch(const Request& request);

private:
    std::mutex m_handlersMutex;
    std::map<std::string, Handler> m_handlers; //< key = METHOD + " " + PATH

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    int m_port = 0;

    int m_listenFd = -1;
    std::thread m_thread;
};

} // namespace nx::vms_server_plugins::analytics::stub::common

