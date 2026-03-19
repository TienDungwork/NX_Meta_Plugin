// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server.h"

#include <cerrno>
#include <cstring>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace nx::vms_server_plugins::analytics::stub::common {

namespace {

static std::string statusText(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

static bool startsWith(const std::string& s, const char* prefix)
{
    const size_t n = std::strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

static void writeAll(int fd, const std::string& data)
{
    const char* p = data.data();
    size_t left = data.size();
    while (left > 0)
    {
        const ssize_t n = ::send(fd, p, left, MSG_NOSIGNAL);
        if (n <= 0)
            return;
        p += (size_t) n;
        left -= (size_t) n;
    }
}

static bool readLine(int fd, std::string* outLine)
{
    outLine->clear();
    char c = 0;
    while (true)
    {
        const ssize_t n = ::recv(fd, &c, 1, 0);
        if (n <= 0)
            return false;
        if (c == '\r')
            continue;
        if (c == '\n')
            return true;
        outLine->push_back(c);
        if (outLine->size() > 8192)
            return false;
    }
}

static bool readBytes(int fd, size_t count, std::string* out)
{
    out->clear();
    out->resize(count);
    size_t offset = 0;
    while (offset < count)
    {
        const ssize_t n = ::recv(fd, out->data() + offset, count - offset, 0);
        if (n <= 0)
            return false;
        offset += (size_t) n;
    }
    return true;
}

static HttpServer::Response makeText(int code, std::string text)
{
    HttpServer::Response r;
    r.statusCode = code;
    r.contentType = "text/plain; charset=utf-8";
    r.body = std::move(text);
    return r;
}

} // namespace

HttpServer::HttpServer() = default;

HttpServer::~HttpServer()
{
    stop();
}

void HttpServer::setHandler(std::string method, std::string path, Handler handler)
{
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    const std::string key = std::move(method) + " " + std::move(path);
    m_handlers[key] = std::move(handler);
}

bool HttpServer::start(int port)
{
    if (port <= 0 || port > 65535)
        return false;

    if (m_running.load() && m_port == port)
        return true;

    stop();

    m_port = port;
    m_stopRequested.store(false);

    m_listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0)
        return false;

    int opt = 1;
    ::setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t) port);

    if (::bind(m_listenFd, (sockaddr*) &addr, sizeof(addr)) != 0)
    {
        closeListenSocket();
        return false;
    }

    if (::listen(m_listenFd, 16) != 0)
    {
        closeListenSocket();
        return false;
    }

    m_running.store(true);
    m_thread = std::thread([this]{ threadMain(); });
    return true;
}

void HttpServer::stop()
{
    if (!m_running.exchange(false))
        return;

    m_stopRequested.store(true);
    closeListenSocket(); //< wake accept()

    if (m_thread.joinable())
        m_thread.join();
}

void HttpServer::closeListenSocket()
{
    if (m_listenFd >= 0)
    {
        ::close(m_listenFd);
        m_listenFd = -1;
    }
}

HttpServer::Response HttpServer::dispatch(const Request& request)
{
    std::lock_guard<std::mutex> lock(m_handlersMutex);
    const std::string key = request.method + " " + request.path;
    auto it = m_handlers.find(key);
    if (it == m_handlers.end())
        return makeText(404, "Not found");
    return it->second(request);
}

void HttpServer::threadMain()
{
    while (!m_stopRequested.load())
    {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        const int clientFd = ::accept(m_listenFd, (sockaddr*) &clientAddr, &len);
        if (clientFd < 0)
        {
            if (m_stopRequested.load())
                return;
            continue;
        }

        // Parse request line.
        Request request;
        std::string line;
        if (!readLine(clientFd, &line))
        {
            ::close(clientFd);
            continue;
        }

        // Expect: METHOD SP PATH[?QUERY] SP HTTP/1.1
        const size_t sp1 = line.find(' ');
        const size_t sp2 = (sp1 == std::string::npos) ? std::string::npos : line.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos)
        {
            writeAll(clientFd, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
            ::close(clientFd);
            continue;
        }

        request.method = line.substr(0, sp1);
        std::string target = line.substr(sp1 + 1, sp2 - sp1 - 1);
        const size_t q = target.find('?');
        request.path = (q == std::string::npos) ? target : target.substr(0, q);
        request.query = (q == std::string::npos) ? "" : target.substr(q + 1);

        // Headers.
        size_t contentLength = 0;
        while (readLine(clientFd, &line))
        {
            if (line.empty())
                break;
            if (startsWith(line, "Content-Length:"))
            {
                const char* p = line.c_str() + std::strlen("Content-Length:");
                while (*p == ' ') ++p;
                contentLength = (size_t) std::strtoul(p, nullptr, 10);
            }
        }

        if (contentLength > 0)
        {
            if (!readBytes(clientFd, contentLength, &request.body))
            {
                ::close(clientFd);
                continue;
            }
        }

        Response response;
        try
        {
            response = dispatch(request);
        }
        catch (...)
        {
            response = makeText(500, "Unhandled exception");
        }

        const std::string body = response.body;
        std::string headers =
            "HTTP/1.1 " + std::to_string(response.statusCode) + " " + statusText(response.statusCode) + "\r\n"
            "Content-Type: " + response.contentType + "\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n";

        writeAll(clientFd, headers);
        writeAll(clientFd, body);
        ::close(clientFd);
    }
}

} // namespace nx::vms_server_plugins::analytics::stub::common

