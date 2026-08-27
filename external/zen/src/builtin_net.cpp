/* =========================================================
** builtin_net.cpp — "net" module for Zen
**
** TCP/UDP networking via POSIX sockets.
**
** Usage:
**   import net
**   var sock = net.tcp_connect("example.com", 80)
**   net.send(sock, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")
**   var resp = net.recv(sock, 4096)
**   net.close(sock)
**
**   # UDP
**   var u = net.udp_create(9000)
**   net.sendto(u, "hello", "127.0.0.1", 9001)
**   var pkt = net.recvfrom(u)  # [data, ip, port]
**
**   # Non-blocking + poll
**   net.set_blocking(sock, False)
**   if net.poll(sock, 0):
**       data = net.recv(sock)
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
static bool wsa_init_done = false;
static void ensure_wsa()
{
    if (!wsa_init_done)
    {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
        wsa_init_done = true;
    }
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
static void ensure_wsa() {}
#endif

namespace zen
{

    /* =========================================================
    ** Socket handle table
    ** ========================================================= */

    static const int MAX_SOCKETS = 64;

    struct SocketSlot
    {
        int fd;
        bool in_use;
        bool is_udp;
    };

    static SocketSlot sock_table[MAX_SOCKETS];
    static bool sock_table_init = false;

    static void init_sock_table()
    {
        if (!sock_table_init)
        {
            for (int i = 0; i < MAX_SOCKETS; i++)
            {
                sock_table[i].fd = -1;
                sock_table[i].in_use = false;
                sock_table[i].is_udp = false;
            }
            sock_table_init = true;
        }
    }

    static int alloc_sock(int fd, bool udp)
    {
        init_sock_table();
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            if (!sock_table[i].in_use)
            {
                sock_table[i].fd = fd;
                sock_table[i].in_use = true;
                sock_table[i].is_udp = udp;
                return i + 1; /* 1-based handle */
            }
        }
        return -1;
    }

    static int get_sock_fd(int handle)
    {
        if (handle < 1 || handle > MAX_SOCKETS)
            return -1;
        SocketSlot &s = sock_table[handle - 1];
        return s.in_use ? s.fd : -1;
    }

    static void free_sock(int handle)
    {
        if (handle >= 1 && handle <= MAX_SOCKETS)
        {
            sock_table[handle - 1].fd = -1;
            sock_table[handle - 1].in_use = false;
        }
    }

    /* =========================================================
    ** resolve(host) → ip string or nil
    ** ========================================================= */
    static int nat_net_resolve(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            args[0] = val_nil();
            return 1;
        }
        ensure_wsa();

        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(safe_string_chars(args[0]), NULL, &hints, &res) != 0)
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
    ** tcp_connect(host, port) → handle or nil
    ** ========================================================= */
    static int nat_net_tcp_connect(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[0]) || !is_int(args[1]))
        {
            args[0] = val_nil();
            return 1;
        }
        ensure_wsa();

        const char *host = as_cstring(args[0]);
        int port = (int)args[1].as.integer;

        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[8];
        snprintf(port_str, sizeof(port_str), "%d", port);

        if (getaddrinfo(host, port_str, &hints, &res) != 0)
        {
            args[0] = val_nil();
            return 1;
        }

        int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0)
        {
            freeaddrinfo(res);
            args[0] = val_nil();
            return 1;
        }

        if (connect(fd, res->ai_addr, res->ai_addrlen) < 0)
        {
            freeaddrinfo(res);
            close(fd);
            args[0] = val_nil();
            return 1;
        }

        freeaddrinfo(res);

        int handle = alloc_sock(fd, false);
        if (handle < 0)
        {
            close(fd);
            vm->runtime_error("net: too many open sockets");
            return -1;
        }

        args[0] = val_int(handle);
        return 1;
    }

    /* =========================================================
    ** tcp_listen(port, backlog?) → handle or nil
    ** ========================================================= */
    static int nat_net_tcp_listen(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            args[0] = val_nil();
            return 1;
        }
        ensure_wsa();

        int port = (int)args[0].as.integer;
        int backlog = (nargs >= 2 && is_int(args[1])) ? (int)args[1].as.integer : 16;

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            args[0] = val_nil();
            return 1;
        }

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons((uint16_t)port);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            close(fd);
            args[0] = val_nil();
            return 1;
        }

        if (listen(fd, backlog) < 0)
        {
            close(fd);
            args[0] = val_nil();
            return 1;
        }

        int handle = alloc_sock(fd, false);
        if (handle < 0)
        {
            close(fd);
            vm->runtime_error("net: too many open sockets");
            return -1;
        }

        args[0] = val_int(handle);
        return 1;
    }

    /* =========================================================
    ** tcp_accept(server_handle) → handle or nil
    ** ========================================================= */
    static int nat_net_tcp_accept(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            args[0] = val_nil();
            return 1;
        }

        int server_fd = get_sock_fd((int)args[0].as.integer);
        if (server_fd < 0)
        {
            args[0] = val_nil();
            return 1;
        }

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            args[0] = val_nil();
            return 1;
        }

        int handle = alloc_sock(client_fd, false);
        if (handle < 0)
        {
            close(client_fd);
            vm->runtime_error("net: too many open sockets");
            return -1;
        }

        args[0] = val_int(handle);
        return 1;
    }

    /* =========================================================
    ** udp_create(port?) → handle or nil
    ** ========================================================= */
    static int nat_net_udp_create(VM *vm, Value *args, int nargs)
    {
        ensure_wsa();
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
        {
            args[0] = val_nil();
            return 1;
        }

        /* Bind if port specified */
        if (nargs >= 1 && is_int(args[0]))
        {
            int port = (int)args[0].as.integer;
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons((uint16_t)port);

            if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            {
                close(fd);
                args[0] = val_nil();
                return 1;
            }
        }

        int handle = alloc_sock(fd, true);
        if (handle < 0)
        {
            close(fd);
            vm->runtime_error("net: too many open sockets");
            return -1;
        }

        args[0] = val_int(handle);
        return 1;
    }

    /* =========================================================
    ** send(handle, data) → int (bytes sent) or -1
    ** ========================================================= */
    static int nat_net_send(VM * /*vm*/, Value *args, int nargs)
    {
        if (nargs < 2 || !is_int(args[0]) || !is_string(args[1]))
        {
            args[0] = val_int(-1);
            return 1;
        }

        int fd = get_sock_fd((int)args[0].as.integer);
        if (fd < 0)
        {
            args[0] = val_int(-1);
            return 1;
        }

        ObjString *data = as_string(args[1]);
        int sent = ::send(fd, data->chars, (size_t)data->length, MSG_NOSIGNAL);
        args[0] = val_int((int64_t)sent);
        return 1;
    }

    /* =========================================================
    ** recv(handle, max_bytes?) → string or nil
    ** ========================================================= */
    static int nat_net_recv(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            args[0] = val_nil();
            return 1;
        }

        int fd = get_sock_fd((int)args[0].as.integer);
        if (fd < 0)
        {
            args[0] = val_nil();
            return 1;
        }

        int max_bytes = 4096;
        if (nargs >= 2 && is_int(args[1]))
            max_bytes = (int)args[1].as.integer;
        if (max_bytes <= 0 || max_bytes > 16 * 1024 * 1024)
            max_bytes = 4096;

        char *buf = (char *)malloc((size_t)max_bytes);
        if (!buf)
        {
            args[0] = val_nil();
            return 1;
        }

        int n = ::recv(fd, buf, (size_t)max_bytes, 0);
        if (n <= 0)
        {
            free(buf);
            args[0] = val_nil();
            return 1;
        }

        args[0] = val_obj((Obj *)vm->make_string(buf, (int)n));
        free(buf);
        return 1;
    }

    /* =========================================================
    ** sendto(handle, data, host, port) → int
    ** ========================================================= */
    static int nat_net_sendto(VM * /*vm*/, Value *args, int nargs)
    {
        if (nargs < 4 || !is_int(args[0]) || !is_string(args[1]) ||
            !is_string(args[2]) || !is_int(args[3]))
        {
            args[0] = val_int(-1);
            return 1;
        }

        int fd = get_sock_fd((int)args[0].as.integer);
        if (fd < 0)
        {
            args[0] = val_int(-1);
            return 1;
        }

        ObjString *data = as_string(args[1]);
        const char *host = as_cstring(args[2]);
        int port = (int)args[3].as.integer;

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)port);
        inet_pton(AF_INET, host, &addr.sin_addr);

        int sent = ::sendto(fd, data->chars, (size_t)data->length, 0,
                                (struct sockaddr *)&addr, sizeof(addr));
        args[0] = val_int((int64_t)sent);
        return 1;
    }

    /* =========================================================
    ** recvfrom(handle, max_bytes?) → [data, ip, port] or nil
    ** ========================================================= */
    static int nat_net_recvfrom(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            args[0] = val_nil();
            return 1;
        }

        int fd = get_sock_fd((int)args[0].as.integer);
        if (fd < 0)
        {
            args[0] = val_nil();
            return 1;
        }

        int max_bytes = 4096;
        if (nargs >= 2 && is_int(args[1]))
            max_bytes = (int)args[1].as.integer;
        if (max_bytes <= 0 || max_bytes > 65536)
            max_bytes = 4096;

        char *buf = (char *)malloc((size_t)max_bytes);
        if (!buf)
        {
            args[0] = val_nil();
            return 1;
        }

        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        int n = ::recvfrom(fd, buf, (size_t)max_bytes, 0,
                               (struct sockaddr *)&from_addr, &from_len);
        if (n <= 0)
        {
            free(buf);
            args[0] = val_nil();
            return 1;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from_addr.sin_addr, ip, sizeof(ip));
        int port = ntohs(from_addr.sin_port);

        GC *gc = &vm->get_gc();
        ObjArray *result = new_array(gc);
        array_push(gc, result, val_obj((Obj *)vm->make_string(buf, (int)n)));
        array_push(gc, result, val_obj((Obj *)vm->make_string(ip, (int)strlen(ip))));
        array_push(gc, result, val_int(port));

        free(buf);
        args[0] = val_obj((Obj *)result);
        return 1;
    }

    /* =========================================================
    ** set_blocking(handle, blocking) → bool
    ** ========================================================= */
    static int nat_net_set_blocking(VM * /*vm*/, Value *args, int nargs)
    {
        if (nargs < 2 || !is_int(args[0]) || !is_bool(args[1]))
        {
            args[0] = val_bool(false);
            return 1;
        }

        int fd = get_sock_fd((int)args[0].as.integer);
        if (fd < 0)
        {
            args[0] = val_bool(false);
            return 1;
        }

#ifdef _WIN32
        u_long mode = args[1].as.boolean ? 0 : 1;
        args[0] = val_bool(ioctlsocket(fd, FIONBIO, &mode) == 0);
#else
        int flags = fcntl(fd, F_GETFL, 0);
        if (args[1].as.boolean)
            flags &= ~O_NONBLOCK;
        else
            flags |= O_NONBLOCK;
        args[0] = val_bool(fcntl(fd, F_SETFL, flags) == 0);
#endif
        return 1;
    }

    /* =========================================================
    ** set_nodelay(handle, nodelay) → bool
    ** ========================================================= */
    static int nat_net_set_nodelay(VM * /*vm*/, Value *args, int nargs)
    {
        if (nargs < 2 || !is_int(args[0]) || !is_bool(args[1]))
        {
            args[0] = val_bool(false);
            return 1;
        }

        int fd = get_sock_fd((int)args[0].as.integer);
        if (fd < 0)
        {
            args[0] = val_bool(false);
            return 1;
        }

        int flag = args[1].as.boolean ? 1 : 0;
        args[0] = val_bool(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                                       (const char *)&flag, sizeof(flag)) == 0);
        return 1;
    }

    /* =========================================================
    ** poll(handle, timeout_ms) → bool (data available)
    ** ========================================================= */
    static int nat_net_poll(VM * /*vm*/, Value *args, int nargs)
    {
        if (nargs < 2 || !is_int(args[0]) || !is_int(args[1]))
        {
            args[0] = val_bool(false);
            return 1;
        }

        int fd = get_sock_fd((int)args[0].as.integer);
        if (fd < 0)
        {
            args[0] = val_bool(false);
            return 1;
        }

#ifdef _WIN32
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval tv;
        int ms = (int)args[1].as.integer;
        tv.tv_sec = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;
        int ret = select(fd + 1, &readfds, NULL, NULL, &tv);
        args[0] = val_bool(ret > 0);
#else
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int ret = ::poll(&pfd, 1, (int)args[1].as.integer);
        args[0] = val_bool(ret > 0 && (pfd.revents & POLLIN));
#endif
        return 1;
    }

    /* =========================================================
    ** close(handle) → bool
    ** ========================================================= */
    static int nat_net_close(VM * /*vm*/, Value *args, int nargs)
    {
        if (nargs < 1 || !is_int(args[0]))
        {
            args[0] = val_bool(false);
            return 1;
        }

        int handle = (int)args[0].as.integer;
        int fd = get_sock_fd(handle);
        if (fd < 0)
        {
            args[0] = val_bool(false);
            return 1;
        }

        close(fd);
        free_sock(handle);
        args[0] = val_bool(true);
        return 1;
    }

    /* =========================================================
    ** Registration
    ** ========================================================= */

    static const NativeReg net_functions[] = {
        {"resolve",      nat_net_resolve,      1},
        {"tcp_connect",  nat_net_tcp_connect,   2},
        {"tcp_listen",   nat_net_tcp_listen,   -1},
        {"tcp_accept",   nat_net_tcp_accept,    1},
        {"udp_create",   nat_net_udp_create,   -1},
        {"send",         nat_net_send,          2},
        {"recv",         nat_net_recv,         -1},
        {"sendto",       nat_net_sendto,        4},
        {"recvfrom",     nat_net_recvfrom,     -1},
        {"set_blocking", nat_net_set_blocking,  2},
        {"set_nodelay",  nat_net_set_nodelay,   2},
        {"poll",         nat_net_poll,           2},
        {"close",        nat_net_close,          1},
    };

    extern const NativeLib zen_lib_net = {
        "net",
        net_functions,
        13,       /* num_functions */
        nullptr,  /* constants */
        0,        /* num_constants */
        nullptr,  /* init_fn */
    };

} /* namespace zen */
