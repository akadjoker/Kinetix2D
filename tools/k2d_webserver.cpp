#include <array>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace
{
#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
void closeSocket(Socket socket) { closesocket(socket); }
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
void closeSocket(Socket socket) { close(socket); }
#endif

const char* mimeType(const fs::path& path)
{
    const std::string extension = path.extension().string();
    if (extension == ".html") return "text/html; charset=utf-8";
    if (extension == ".js") return "application/javascript; charset=utf-8";
    if (extension == ".wasm") return "application/wasm";
    if (extension == ".data" || extension == ".zbc") return "application/octet-stream";
    if (extension == ".json") return "application/json; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".ogg") return "audio/ogg";
    if (extension == ".wav") return "audio/wav";
    return "application/octet-stream";
}

bool sendAll(Socket socket, const char* data, size_t size)
{
    while (size > 0)
    {
        const int sent = send(socket, data, static_cast<int>(size), 0);
        if (sent <= 0)
            return false;
        data += sent;
        size -= static_cast<size_t>(sent);
    }
    return true;
}

bool safeRelativePath(const std::string& target, fs::path& result)
{
    const size_t query = target.find_first_of("?#");
    const std::string raw = target.substr(0, query);
    if (raw.empty() || raw[0] != '/')
        return false;

    result.clear();
    size_t begin = 1;
    while (begin <= raw.size())
    {
        const size_t end = raw.find('/', begin);
        const std::string part = raw.substr(begin, end == std::string::npos ? end : end - begin);
        if (part == ".." || part.find('\\') != std::string::npos || part.find('%') != std::string::npos)
            return false;
        if (!part.empty() && part != ".")
            result /= part;
        if (end == std::string::npos)
            break;
        begin = end + 1;
    }
    return true;
}

void respond(Socket client, int status, const char* text, const char* type = "text/plain; charset=utf-8")
{
    const char* reason = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 404 ? "Not Found" : "Bad Request";
    const std::string body = text ? text : "";
    const std::string header = "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\nContent-Type: " + type +
                               "\r\nContent-Length: " + std::to_string(body.size()) +
                               "\r\nConnection: close\r\nCache-Control: no-cache\r\n\r\n";
    sendAll(client, header.data(), header.size());
    sendAll(client, body.data(), body.size());
}

void handleClient(Socket client, const fs::path& root)
{
    std::array<char, 4096> buffer{};
    const int received = recv(client, buffer.data(), static_cast<int>(buffer.size() - 1), 0);
    if (received <= 0)
        return;
    buffer[static_cast<size_t>(received)] = '\0';
    const std::string request(buffer.data());
    const size_t lineEnd = request.find("\r\n");
    const size_t firstSpace = request.find(' ');
    const size_t secondSpace = firstSpace == std::string::npos ? std::string::npos : request.find(' ', firstSpace + 1);
    if (lineEnd == std::string::npos || firstSpace == std::string::npos || secondSpace == std::string::npos ||
        (request.substr(0, firstSpace) != "GET" && request.substr(0, firstSpace) != "HEAD"))
    {
        respond(client, 400, "Only GET requests are supported\n");
        return;
    }

    fs::path relative;
    if (!safeRelativePath(request.substr(firstSpace + 1, secondSpace - firstSpace - 1), relative))
    {
        respond(client, 403, "Forbidden\n");
        return;
    }
    fs::path file = root / relative;
    std::error_code error;
    if (fs::is_directory(file, error))
        file /= "index.html";
    const fs::path canonicalRoot = fs::weakly_canonical(root, error);
    const fs::path canonicalFile = fs::weakly_canonical(file, error);
    bool insideRoot = !error;
    auto rootIt = canonicalRoot.begin();
    auto fileIt = canonicalFile.begin();
    while (insideRoot && rootIt != canonicalRoot.end())
    {
        insideRoot = fileIt != canonicalFile.end() && *rootIt == *fileIt;
        ++rootIt;
        ++fileIt;
    }
    if (!insideRoot || !fs::is_regular_file(canonicalFile))
    {
        respond(client, 404, "Not found\n");
        return;
    }

    std::ifstream input(canonicalFile, std::ios::binary);
    input.seekg(0, std::ios::end);
    const size_t size = static_cast<size_t>(input.tellg());
    input.seekg(0, std::ios::beg);
    const std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + std::string(mimeType(canonicalFile)) +
                               "\r\nContent-Length: " + std::to_string(size) +
                               "\r\nConnection: close\r\nCache-Control: no-cache\r\n\r\n";
    if (!sendAll(client, header.data(), header.size()))
        return;
    while (input.good())
    {
        const std::streamsize count = input.read(buffer.data(), buffer.size()).gcount();
        if (count <= 0 || !sendAll(client, buffer.data(), static_cast<size_t>(count)))
            return;
    }
}
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
    {
        std::fprintf(stderr, "Usage: k2d_webserver <export-directory> [port]\n");
        return 1;
    }
    const fs::path root = fs::absolute(argv[1]);
    const int port = argc == 3 ? std::atoi(argv[2]) : 8080;
    if (!fs::is_directory(root) || port < 1 || port > 65535)
        return 1;

#if defined(_WIN32)
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        return 1;
#endif
    const Socket server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == kInvalidSocket)
        return 1;
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(server, 16) != 0)
    {
        closeSocket(server);
        return 1;
    }
    std::printf("Kinetix2D Web server: http://127.0.0.1:%d/\n", port);
    for (;;)
    {
        const Socket client = accept(server, nullptr, nullptr);
        if (client != kInvalidSocket)
        {
            handleClient(client, root);
            closeSocket(client);
        }
    }
}
