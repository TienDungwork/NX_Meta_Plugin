#include "http_server.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <nx/kit/debug.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "[HTTP_SERVER] "

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

HttpServer::HttpServer(int port)
    : m_port(port)
    , m_serverSocket(-1)
    , m_running(false)
{
}

HttpServer::~HttpServer()
{
    stop();
}

void HttpServer::setRequestCallback(RequestCallback callback)
{
    m_requestCallback = callback;
}

void HttpServer::start()
{
    if (m_running)
        return;

    m_running = true;
    m_thread = std::thread(&HttpServer::serverThread, this);
    NX_PRINT << "HTTP Server starting on port " << m_port;
}

void HttpServer::stop()
{
    if (!m_running)
        return;

    NX_PRINT << "Stopping HTTP Server";
    m_running = false;

    if (m_serverSocket >= 0)
    {
        shutdown(m_serverSocket, SHUT_RDWR);
        close(m_serverSocket);
        m_serverSocket = -1;
    }

    if (m_thread.joinable())
        m_thread.join();
}

void HttpServer::serverThread()
{
    // Create socket
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverSocket < 0)
    {
        NX_PRINT << "Failed to create socket";
        return;
    }

    // Allow socket reuse
    int opt = 1;
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        NX_PRINT << "setsockopt failed";
        close(m_serverSocket);
        return;
    }

    // Bind to port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_port);

    if (bind(m_serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        NX_PRINT << "Bind failed on port " << m_port;
        close(m_serverSocket);
        return;
    }

    // Listen
    if (listen(m_serverSocket, 10) < 0)
    {
        NX_PRINT << "Listen failed";
        close(m_serverSocket);
        return;
    }

    NX_PRINT << "HTTP Server listening on port " << m_port;

    // Accept connections
    while (m_running)
    {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        int clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0)
        {
            if (m_running)
                NX_PRINT << "Accept failed";
            continue;
        }

        // Handle client in separate thread to avoid blocking
        std::thread(&HttpServer::handleClient, this, clientSocket).detach();
    }
}

void HttpServer::handleClient(int clientSocket)
{
    char buffer[4096] = {0};
    int bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
    
    if (bytesRead <= 0)
    {
        close(clientSocket);
        return;
    }

    std::string request(buffer, bytesRead);
    NX_PRINT << "Received HTTP request:\n" << request;

    // Parse request to get camera_id
    std::string cameraId = parseGetRequest(request);
    
    std::string responseBody;
    int statusCode = 200;

    if (cameraId.empty())
    {
        responseBody = R"({"error": "Missing camera_id parameter"})";
        statusCode = 400;
    }
    else if (!m_requestCallback)
    {
        responseBody = R"({"error": "Request callback not set"})";
        statusCode = 500;
    }
    else
    {
        responseBody = m_requestCallback(cameraId);
        if (responseBody.empty() || responseBody.find("\"error\"") != std::string::npos)
        {
            statusCode = 404;
        }
    }

    std::string response = createHttpResponse(responseBody, statusCode);
    send(clientSocket, response.c_str(), response.length(), 0);
    
    close(clientSocket);
}

std::string HttpServer::parseGetRequest(const std::string& request)
{
    // Parse GET /polygon?camera_id=xxx HTTP/1.1
    size_t getPos = request.find("GET ");
    if (getPos == std::string::npos)
        return "";

    size_t questionPos = request.find("?", getPos);
    if (questionPos == std::string::npos)
        return "";

    size_t cameraIdPos = request.find("camera_id=", questionPos);
    if (cameraIdPos == std::string::npos)
        return "";

    size_t start = cameraIdPos + 10; // Length of "camera_id="
    size_t end = request.find_first_of(" &\r\n", start);
    if (end == std::string::npos)
        end = request.length();

    std::string cameraId = request.substr(start, end - start);
    
    // URL decode if needed (handle %7B = {, %7D = })
    size_t pos = 0;
    while ((pos = cameraId.find("%7B", pos)) != std::string::npos)
    {
        cameraId.replace(pos, 3, "{");
        pos += 1;
    }
    pos = 0;
    while ((pos = cameraId.find("%7D", pos)) != std::string::npos)
    {
        cameraId.replace(pos, 3, "}");
        pos += 1;
    }
    pos = 0;
    while ((pos = cameraId.find("%7b", pos)) != std::string::npos)
    {
        cameraId.replace(pos, 3, "{");
        pos += 1;
    }
    pos = 0;
    while ((pos = cameraId.find("%7d", pos)) != std::string::npos)
    {
        cameraId.replace(pos, 3, "}");
        pos += 1;
    }

    NX_PRINT << "Parsed camera_id: " << cameraId;
    return cameraId;
}

std::string HttpServer::createHttpResponse(const std::string& body, int statusCode)
{
    std::string statusText = (statusCode == 200) ? "OK" : 
                             (statusCode == 400) ? "Bad Request" :
                             (statusCode == 404) ? "Not Found" : "Internal Server Error";

    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << body;

    return response.str();
}

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
