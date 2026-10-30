#include <ct/string.hpp>
#include <platform.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

const char* mimeType(const char* path)
{
    const ct::String extension(path_extension(path));
    if (extension == ".html") return "text/html; charset=utf-8";
    if (extension == ".js") return "application/javascript; charset=utf-8";
    if (extension == ".wasm") return "application/wasm";
    if (extension == ".data" || extension == ".zbc" || extension == ".kpak") return "application/octet-stream";
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
#if defined(MSG_NOSIGNAL)
    constexpr int noSignal = MSG_NOSIGNAL;
#else
    constexpr int noSignal = 0;
#endif
    while (size > 0)
    {
        // Browsers can cancel a fetch as soon as they have the data they
        // need, which turns the rest of the send into an error here.
        const auto sent = send(socket, data, static_cast<int>(size), noSignal);
        if (sent <= 0)
            return false;
        data += sent;
        size -= static_cast<size_t>(sent);
    }
    return true;
}

bool safeRelativePath(const ct::String& target, ct::String& result)
{
    const size_t query = target.find_first_of("?#");
    const ct::String raw = target.substr(0, query);
    if (raw.empty() || raw[0] != '/')
        return false;

    result.clear();
    size_t begin = 1;
    while (begin <= raw.size())
    {
        const size_t end = raw.find('/', begin);
        const ct::String part = raw.substr(begin, end == ct::String::npos ? end : end - begin);
        if (part == ".." || part.contains('\\') || part.contains('%'))
            return false;
        if (!part.empty() && part != ".")
        {
            if (!result.empty())
                result += '/';
            result += part;
        }
        if (end == ct::String::npos)
            break;
        begin = end + 1;
    }
    return true;
}

void respond(Socket client, int status, const char* text, const char* type = "text/plain; charset=utf-8")
{
    const char* reason = status == 200 ? "OK" : status == 403 ? "Forbidden" : status == 404 ? "Not Found" : "Bad Request";
    const size_t bodySize = text ? std::strlen(text) : 0;
    char header[512];
    const int headerSize = std::snprintf(header, sizeof header,
                                         "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                                         "Connection: close\r\nCache-Control: no-cache\r\n\r\n",
                                         status, reason, type, bodySize);
    sendAll(client, header, static_cast<size_t>(headerSize));
    sendAll(client, text ? text : "", bodySize);
}

ct::String absolutePath(const char* path)
{
    char absolute[4096];
    char normalized[4096];
    if (!path_absolute(absolute, sizeof absolute, path) || !path_normalize(normalized, sizeof normalized, absolute))
        return ct::String();
    return ct::String(normalized);
}

bool insideRoot(const ct::String& root, const ct::String& file)
{
    if (!file.starts_with(root))
        return false;
    return file.size() == root.size() || root.ends_with("/") || file[root.size()] == '/';
}

void handleClient(Socket client, const ct::String& root)
{
    char buffer[4096] = {};
    const int received = recv(client, buffer, static_cast<int>(sizeof buffer - 1), 0);
    if (received <= 0)
        return;
    buffer[static_cast<size_t>(received)] = '\0';
    const ct::String request(buffer);
    const size_t lineEnd = request.find("\r\n");
    const size_t firstSpace = request.find(' ');
    const size_t secondSpace = firstSpace == ct::String::npos ? ct::String::npos : request.find(' ', firstSpace + 1);
    if (lineEnd == ct::String::npos || firstSpace == ct::String::npos || secondSpace == ct::String::npos ||
        (request.substr(0, firstSpace) != "GET" && request.substr(0, firstSpace) != "HEAD"))
    {
        respond(client, 400, "Only GET requests are supported\n");
        return;
    }

    ct::String relative;
    if (!safeRelativePath(request.substr(firstSpace + 1, secondSpace - firstSpace - 1), relative))
    {
        respond(client, 403, "Forbidden\n");
        return;
    }
    char joined[4096];
    if (!path_join(joined, sizeof joined, root.c_str(), relative.c_str()))
    {
        respond(client, 404, "Not found\n");
        return;
    }
    ct::String file(joined);
    PathInfo info = {};
    if (fs_get_path_info(file.c_str(), &info) && info.type == PATH_TYPE_DIRECTORY)
        file += "/index.html";
    const ct::String canonicalFile = absolutePath(file.c_str());
    if (canonicalFile.empty() || !insideRoot(root, canonicalFile) || !fs_get_path_info(canonicalFile.c_str(), &info) ||
        info.type != PATH_TYPE_FILE)
    {
        respond(client, 404, "Not found\n");
        return;
    }

    size_t size = 0;
    uint8_t* data = file_read(canonicalFile.c_str(), &size);
    if (!data)
    {
        respond(client, 404, "Not found\n");
        return;
    }
    char header[512];
    const int headerSize = std::snprintf(header, sizeof header,
                                         "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                                         "Connection: close\r\nCache-Control: no-cache\r\n\r\n",
                                         mimeType(canonicalFile.c_str()), size);
    if (sendAll(client, header, static_cast<size_t>(headerSize)))
        sendAll(client, reinterpret_cast<const char*>(data), size);
    fs_free(data);
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3)
    {
        std::fprintf(stderr, "Usage: k2d_webserver <export-directory> [port]\n");
        return 1;
    }
    const ct::String root = absolutePath(argv[1]);
    const int port = argc == 3 ? std::atoi(argv[2]) : 8080;
    if (root.empty() || !dir_exists(root.c_str()) || port < 1 || port > 65535)
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
