/* =========================================================
** builtin_http.cpp — "http" module for Zen
**
** High-level HTTP client (like Python's requests).
** Built on raw POSIX sockets, no external deps.
** HTTP/1.1, Connection: close. No HTTPS (yet).
**
** Usage:
**   import http
**   var r = http.get("http://example.com")
**   print(r["status"])   # 200
**   print(r["body"])     # HTML...
**
**   var r = http.post("http://api.example.com/data",
**                     '{"key":"value"}', "application/json")
**   print(r["body"])
**
**   http.download("http://example.com/file.zip", "./file.zip")
**   print(http.ping("google.com", 80))
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define MSG_NOSIGNAL 0
#define close closesocket
static void ensure_wsa_http()
{
    static bool done = false;
    if (!done) { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); done = true; }
}
#else
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
static void ensure_wsa_http() {}
#endif

namespace zen
{

    /* =========================================================
    ** Internal helpers
    ** ========================================================= */

    struct ParsedURL
    {
        char host[256];
        char path[2048];
        int port;
        bool valid;
    };

    static ParsedURL parse_url(const char *url)
    {
        ParsedURL u;
        u.valid = false;
        u.port = 80;
        u.host[0] = '\0';
        u.path[0] = '/';
        u.path[1] = '\0';

        const char *p = url;
        if (strncmp(p, "http://", 7) == 0)
            p += 7;
        else if (strncmp(p, "https://", 8) == 0)
            return u; /* HTTPS not supported yet */

        const char *slash = strchr(p, '/');
        int host_len = slash ? (int)(slash - p) : (int)strlen(p);
        if (host_len <= 0 || host_len >= 256)
            return u;

        char host_port[256];
        memcpy(host_port, p, (size_t)host_len);
        host_port[host_len] = '\0';

        char *colon = strchr(host_port, ':');
        if (colon)
        {
            *colon = '\0';
            u.port = atoi(colon + 1);
            if (u.port <= 0 || u.port > 65535)
                return u;
        }
        strcpy(u.host, host_port);

        if (slash)
        {
            int path_len = (int)strlen(slash);
            if (path_len >= 2048)
                path_len = 2047;
            memcpy(u.path, slash, (size_t)path_len);
            u.path[path_len] = '\0';
        }

        u.valid = true;
        return u;
    }

    static int http_connect(const char *host, int port, int timeout_sec)
    {
        ensure_wsa_http();
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%d", port);

        if (getaddrinfo(host, port_str, &hints, &res) != 0)
            return -1;

        int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0)
        {
            freeaddrinfo(res);
            return -1;
        }

        /* Set timeout */
        struct timeval tv;
        tv.tv_sec = timeout_sec;
        tv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));

        if (connect(fd, res->ai_addr, res->ai_addrlen) < 0)
        {
            freeaddrinfo(res);
            close(fd);
            return -1;
        }

        freeaddrinfo(res);
        return fd;
    }

    static bool send_all(int fd, const char *data, int len)
    {
        int sent = 0;
        while (sent < len)
        {
            int n = ::send(fd, data + sent, (size_t)(len - sent), MSG_NOSIGNAL);
            if (n <= 0)
                return false;
            sent += (int)n;
        }
        return true;
    }

    static char *recv_all(int fd, int *out_len, int max_size)
    {
        int cap = 8192;
        int len = 0;
        char *buf = (char *)malloc((size_t)cap);
        if (!buf)
            return NULL;

        for (;;)
        {
            if (len + 4096 > cap)
            {
                if (cap >= max_size)
                    break;
                cap *= 2;
                if (cap > max_size)
                    cap = max_size;
                char *nb = (char *)realloc(buf, (size_t)cap);
                if (!nb)
                    break;
                buf = nb;
            }
            int n = ::recv(fd, buf + len, (size_t)(cap - len), 0);
            if (n <= 0)
                break;
            len += (int)n;
        }

        *out_len = len;
        return buf;
    }

    static int find_header_end(const char *data, int len)
    {
        for (int i = 0; i + 3 < len; i++)
        {
            if (data[i] == '\r' && data[i + 1] == '\n' &&
                data[i + 2] == '\r' && data[i + 3] == '\n')
                return i;
        }
        return -1;
    }

    static int parse_status_code(const char *data, int len)
    {
        int i = 0;
        while (i < len && data[i] != ' ')
            i++;
        i++;
        if (i + 3 > len)
            return 0;
        return (data[i] - '0') * 100 + (data[i + 1] - '0') * 10 + (data[i + 2] - '0');
    }

    /* Build response map: {status, body, headers, success, url} */
    static Value build_response(VM *vm, const char *raw, int raw_len,
                                const char *url, int url_len)
    {
        GC *gc = &vm->get_gc();
        ObjMap *resp = new_map(gc);

        int header_end = find_header_end(raw, raw_len);
        if (header_end < 0)
        {
            map_set(gc, resp, val_obj((Obj *)vm->make_string("status", 6)), val_int(0));
            map_set(gc, resp, val_obj((Obj *)vm->make_string("body", 4)),
                    val_obj((Obj *)vm->make_string("", 0)));
            map_set(gc, resp, val_obj((Obj *)vm->make_string("success", 7)), val_bool(false));
            map_set(gc, resp, val_obj((Obj *)vm->make_string("url", 3)),
                    val_obj((Obj *)vm->make_string(url, url_len)));
            return val_obj((Obj *)resp);
        }

        int status = parse_status_code(raw, header_end);
        const char *body = raw + header_end + 4;
        int body_len = raw_len - header_end - 4;

        map_set(gc, resp, val_obj((Obj *)vm->make_string("status", 6)), val_int(status));
        map_set(gc, resp, val_obj((Obj *)vm->make_string("body", 4)),
                val_obj((Obj *)vm->make_string(body, body_len)));
        map_set(gc, resp, val_obj((Obj *)vm->make_string("success", 7)),
                val_bool(status >= 200 && status < 300));
        map_set(gc, resp, val_obj((Obj *)vm->make_string("url", 3)),
                val_obj((Obj *)vm->make_string(url, url_len)));

        /* Parse headers into sub-map */
        ObjMap *hdrs = new_map(gc);
        const char *line = raw;
        while (line < raw + header_end && *line != '\n')
            line++;
        line++;

        while (line < raw + header_end)
        {
            const char *line_end = line;
            while (line_end < raw + header_end && *line_end != '\r')
                line_end++;

            const char *colon = line;
            while (colon < line_end && *colon != ':')
                colon++;

            if (colon < line_end)
            {
                int key_len = (int)(colon - line);
                const char *val_start = colon + 1;
                while (val_start < line_end && *val_start == ' ')
                    val_start++;
                int val_len = (int)(line_end - val_start);

                map_set(gc, hdrs,
                        val_obj((Obj *)vm->make_string(line, key_len)),
                        val_obj((Obj *)vm->make_string(val_start, val_len)));
            }

            line = line_end;
            if (line + 2 <= raw + header_end && line[0] == '\r' && line[1] == '\n')
                line += 2;
            else
                break;
        }
        map_set(gc, resp, val_obj((Obj *)vm->make_string("headers", 7)),
                val_obj((Obj *)hdrs));

        return val_obj((Obj *)resp);
    }

    /* =========================================================
    ** http.get(url, timeout?) → map
    ** ========================================================= */
    static int nat_http_get(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("http.get() expects (url_string)");
            return -1;
        }

        ObjString *url_str = as_string(args[0]);
        int timeout = (nargs >= 2 && is_int(args[1])) ? (int)args[1].as.integer : 30;

        ParsedURL u = parse_url(url_str->chars);
        if (!u.valid)
        {
            vm->runtime_error("http.get(): invalid or unsupported URL");
            return -1;
        }

        int fd = http_connect(u.host, u.port, timeout);
        if (fd < 0)
        {
            args[0] = val_nil();
            return 1;
        }

        char req[4096];
        int req_len = snprintf(req, sizeof(req),
                               "GET %s HTTP/1.1\r\n"
                               "Host: %s\r\n"
                               "User-Agent: Zen/1.0\r\n"
                               "Accept: */*\r\n"
                               "Connection: close\r\n"
                               "\r\n",
                               u.path, u.host);

        if (!send_all(fd, req, req_len))
        {
            close(fd);
            args[0] = val_nil();
            return 1;
        }

        int resp_len = 0;
        char *resp = recv_all(fd, &resp_len, 16 * 1024 * 1024);
        close(fd);

        if (!resp || resp_len == 0)
        {
            free(resp);
            args[0] = val_nil();
            return 1;
        }

        args[0] = build_response(vm, resp, resp_len, url_str->chars, url_str->length);
        free(resp);
        return 1;
    }

    /* =========================================================
    ** http.post(url, body, content_type?, timeout?) → map
    ** ========================================================= */
    static int nat_http_post(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[0]) || !is_string(args[1]))
        {
            vm->runtime_error("http.post() expects (url, body_string, [content_type], [timeout])");
            return -1;
        }

        ObjString *url_str = as_string(args[0]);
        ObjString *body = as_string(args[1]);
        const char *ctype = "application/x-www-form-urlencoded";
        if (nargs >= 3 && is_string(args[2]))
            ctype = as_cstring(args[2]);
        int timeout = (nargs >= 4 && is_int(args[3])) ? (int)args[3].as.integer : 30;

        ParsedURL u = parse_url(url_str->chars);
        if (!u.valid)
        {
            vm->runtime_error("http.post(): invalid or unsupported URL");
            return -1;
        }

        int fd = http_connect(u.host, u.port, timeout);
        if (fd < 0)
        {
            args[0] = val_nil();
            return 1;
        }

        char hdr[4096];
        int hdr_len = snprintf(hdr, sizeof(hdr),
                               "POST %s HTTP/1.1\r\n"
                               "Host: %s\r\n"
                               "User-Agent: Zen/1.0\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %d\r\n"
                               "Connection: close\r\n"
                               "\r\n",
                               u.path, u.host, ctype, body->length);

        if (!send_all(fd, hdr, hdr_len) || !send_all(fd, body->chars, body->length))
        {
            close(fd);
            args[0] = val_nil();
            return 1;
        }

        int resp_len = 0;
        char *resp = recv_all(fd, &resp_len, 16 * 1024 * 1024);
        close(fd);

        if (!resp || resp_len == 0)
        {
            free(resp);
            args[0] = val_nil();
            return 1;
        }

        args[0] = build_response(vm, resp, resp_len, url_str->chars, url_str->length);
        free(resp);
        return 1;
    }

    /* =========================================================
    ** http.download(url, filepath, timeout?) → bool
    ** ========================================================= */
    static int nat_http_download(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[0]) || !is_string(args[1]))
        {
            vm->runtime_error("http.download() expects (url, filepath)");
            return -1;
        }

        ObjString *url_str = as_string(args[0]);
        const char *filepath = as_cstring(args[1]);
        int timeout = (nargs >= 3 && is_int(args[2])) ? (int)args[2].as.integer : 60;

        ParsedURL u = parse_url(url_str->chars);
        if (!u.valid)
        {
            args[0] = val_bool(false);
            return 1;
        }

        int fd = http_connect(u.host, u.port, timeout);
        if (fd < 0)
        {
            args[0] = val_bool(false);
            return 1;
        }

        char req[4096];
        int req_len = snprintf(req, sizeof(req),
                               "GET %s HTTP/1.1\r\n"
                               "Host: %s\r\n"
                               "User-Agent: Zen/1.0\r\n"
                               "Connection: close\r\n"
                               "\r\n",
                               u.path, u.host);

        if (!send_all(fd, req, req_len))
        {
            close(fd);
            args[0] = val_bool(false);
            return 1;
        }

        FILE *fp = fopen(filepath, "wb");
        if (!fp)
        {
            close(fd);
            args[0] = val_bool(false);
            return 1;
        }

        char buf[8192];
        bool headers_done = false;
        char header_buf[16384];
        int header_buf_len = 0;

        for (;;)
        {
            int n = ::recv(fd, buf, sizeof(buf), 0);
            if (n <= 0)
                break;

            if (!headers_done)
            {
                int space = (int)sizeof(header_buf) - header_buf_len;
                int copy = (int)n < space ? (int)n : space;
                memcpy(header_buf + header_buf_len, buf, (size_t)copy);
                header_buf_len += copy;

                int end = find_header_end(header_buf, header_buf_len);
                if (end >= 0)
                {
                    headers_done = true;
                    int body_start = end + 4;
                    int body_in_buf = header_buf_len - body_start;
                    if (body_in_buf > 0)
                        fwrite(header_buf + body_start, 1, (size_t)body_in_buf, fp);
                    if (copy < (int)n)
                        fwrite(buf + copy, 1, (size_t)((int)n - copy), fp);
                }
            }
            else
            {
                fwrite(buf, 1, (size_t)n, fp);
            }
        }

        fclose(fp);
        close(fd);
        args[0] = val_bool(headers_done);
        return 1;
    }

    /* =========================================================
    ** http.ping(host, port?, timeout?) → bool
    ** ========================================================= */
    static int nat_http_ping(VM * /*vm*/, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            args[0] = val_bool(false);
            return 1;
        }

        const char *host = as_cstring(args[0]);
        int port = (nargs >= 2 && is_int(args[1])) ? (int)args[1].as.integer : 80;
        int timeout = (nargs >= 3 && is_int(args[2])) ? (int)args[2].as.integer : 2;

        int fd = http_connect(host, port, timeout);
        if (fd < 0)
        {
            args[0] = val_bool(false);
            return 1;
        }

        close(fd);
        args[0] = val_bool(true);
        return 1;
    }

    /* =========================================================
    ** http.get_local_ip() → string or nil
    ** ========================================================= */
    static int nat_http_get_local_ip(VM *vm, Value *args, int /*nargs*/)
    {
        ensure_wsa_http();
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            args[0] = val_nil();
            return 1;
        }

        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(hostname, NULL, &hints, &res) != 0)
        {
            args[0] = val_nil();
            return 1;
        }

        char ip[INET_ADDRSTRLEN];
        struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
        inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
        freeaddrinfo(res);

        args[0] = val_obj((Obj *)vm->make_string(ip, (int)strlen(ip)));
        return 1;
    }

    /* =========================================================
    ** Registration
    ** ========================================================= */

    static const NativeReg http_functions[] = {
        {"get",          nat_http_get,          -1},
        {"post",         nat_http_post,         -1},
        {"download",     nat_http_download,     -1},
        {"ping",         nat_http_ping,         -1},
        {"get_local_ip", nat_http_get_local_ip,  0},
    };

    extern const NativeLib zen_lib_http = {
        "http",
        http_functions,
        5,        /* num_functions */
        nullptr,  /* constants */
        0,        /* num_constants */
        nullptr,  /* init_fn */
    };

} /* namespace zen */
