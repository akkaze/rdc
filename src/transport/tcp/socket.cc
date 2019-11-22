#pragma once
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <cstring>
#include <string>
#include "core/base.h"
#include "core/exception.h"
#include "core/logging.h"
#include "transport/tcp/socket.h"
#include "utils/string_utils.h"
#ifdef _WIN32
#define THROW_SOCKET_ERROR(msg)                             \
    THROW_EXCEPTION(SocketError, rdc::str_utils::SPrintf(   \
                "Socket %s Error:WSAError-code=%d",         \
                msg, WSAGetLastError());
#else
#define THROW_SOCKET_ERROR(msg) \
    THROW_EXCEPTION(            \
        SocketError,            \
        rdc::str_utils::SPrintf("Socket %s Error: %s", msg, strerror(errno)));
#endif

namespace rdc {
SockAddr::SockAddr() {
    std::memset(&addr, 0, sizeof(addr));
}

SockAddr::SockAddr(const char *url, int port) {
    std::memset(&addr, 0, sizeof(addr));
    this->Set(url, port);
}

SockAddr::SockAddr(std::string host, int port) {
    std::memset(&addr, 0, sizeof(addr));
    this->Set(host.c_str(), port);
}

std::string SockAddr::GetHostName() {
    std::string buf;
    buf.resize(256);
    CHECK_S(gethostname(&buf[0], 256) != -1) << "fail to get host name";
    return std::string(buf.c_str());
}

void SockAddr::Set(const char *host, int port) {
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_protocol = SOCK_STREAM;
    addrinfo *res = NULL;
    int sig = getaddrinfo(host, NULL, &hints, &res);
    CHECK_S(sig == 0 && res != NULL) << "cannot obtain address of " << host;
    CHECK_S(res->ai_family == AF_INET) << "Does not support IPv6";
    memcpy(&addr, res->ai_addr, res->ai_addrlen);
    addr.sin_port = htons(port);
    freeaddrinfo(res);
}

int SockAddr::port() const {
    return ntohs(addr.sin_port);
}

std::string SockAddr::AddrStr() const {
    std::string buf;
    buf.resize(256);
#ifdef _WIN32
    const char *s =
        inet_ntop(AF_INET, (PVOID)&addr.sin_addr, &buf[0], buf.length());
#else
    const char *s = inet_ntop(AF_INET, &addr.sin_addr, &buf[0], buf.length());
#endif
    CHECK_S(s != NULL) << "cannot decode address";
    return std::string(s);
}

int Socket::GetLastError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool Socket::LastErrorWouldBlock() {
    int errsv = GetLastError();
#ifdef _WIN32
    return errsv == WSAEWOULDBLOCK;
#else
    return errsv == EAGAIN || errsv == EWOULDBLOCK;
#endif
}

void Socket::Startup() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) == -1) {
        THROW_SOCKET_ERROR("Startup");
    }
    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2) {
        WSACleanup();
        logging::LOG_S(ERROR)
            << "Could not find a usable version of Winsock.dll";
    }
#endif
}

void Socket::Finalize() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void Socket::SetNonBlock(bool non_block) {
#ifdef _WIN32
    u_long mode = non_block ? 1 : 0;
    if (ioctlsocket(sockfd, FIONBIO, &mode) != NO_THROW_SOCKET_ERROR) {
        THROW_SOCKET_ERROR("SetNonBlock");
    }
#else
    int flag = fcntl(sockfd, F_GETFL, 0);
    if (flag == -1) {
        THROW_SOCKET_ERROR("SetNonBlock-1");
    }
    if (non_block) {
        flag |= O_NONBLOCK;
    } else {
        flag &= ~O_NONBLOCK;
    }
    if (fcntl(sockfd, F_SETFL, flag) == -1) {
        THROW_SOCKET_ERROR("SetNonBlock-2");
    }
#endif
}

void Socket::Bind(const SockAddr &addr) {
    if (bind(sockfd, reinterpret_cast<const sockaddr *>(&addr.addr),
             sizeof(addr.addr)) == -1) {
        THROW_SOCKET_ERROR("Bind");
    }
}

int Socket::TryBindHost(int port) {
    SockAddr addr("0.0.0.0", port);
    addr.addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, reinterpret_cast<sockaddr *>(&addr.addr),
             sizeof(addr.addr)) == 0) {
        return port;
    }
#if defined(_WIN32)
    if (WSAGetLastError() != WSAEADDRINUSE) {
        THROW_SOCKET_ERROR("TryBindHost");
    }
#else
    if (errno != EADDRINUSE) {
        THROW_SOCKET_ERROR("TryBindHost");
    }
#endif
    return -1;
}

int Socket::TryBindHost(int start_port, int end_port) {
    for (int port = start_port; port < end_port; ++port) {
        SockAddr addr("0.0.0.0", port);
        if (bind(sockfd, reinterpret_cast<sockaddr *>(&addr.addr),
                 sizeof(addr.addr)) == 0) {
            return port;
        }
#if defined(_WIN32)
        if (WSAGetLastError() != WSAEADDRINUSE) {
            THROW_SOCKET_ERROR("TryBindHost");
        }
#else
        if (errno != EADDRINUSE) {
            THROW_SOCKET_ERROR("TryBindHost");
        }
#endif
    }

    return -1;
}

int Socket::GetSockError() const {
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
                   reinterpret_cast<char *>(&error), &len) != 0) {
        Error("GetSockError");
    }
    return error;
}

bool Socket::BadSocket() const {
    if (IsClosed()) return true;
    int err = GetSockError();
    if (err == EBADF || err == EINTR) return true;
    return false;
}

bool Socket::IsClosed() const {
    return sockfd == INVALID_SOCKET;
}

void Socket::Close() {
    if (sockfd != INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
        sockfd = INVALID_SOCKET;
    } else {
        Error(
            "Socket::Close double close the socket or close without "
            "create");
    }
}

void Socket::Error(const char *msg) {
#ifdef _WIN32
    LOG_F(ERROR, "Socket %s Error:WSAError-code=%d", msg, WSAGetLastError());
#else
    LOG_F(ERROR, "Socket %s Error: %s", msg, strerror(errno));
#endif
}

Socket::Socket(SOCKET sockfd) : sockfd(sockfd) {
}
// constructor
TcpSocket::TcpSocket(bool create) : Socket(INVALID_SOCKET) {
    if (create) Create();
}

TcpSocket::TcpSocket(SOCKET sockfd, bool create) : Socket(sockfd) {
    if (create) Create();
}

void TcpSocket::SetKeepAlive(bool keepalive) {
    int opt = static_cast<int>(keepalive);
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,
                   reinterpret_cast<char *>(&opt), sizeof(opt)) < 0) {
        THROW_SOCKET_ERROR("SetKeepAlive");
    }
}

void TcpSocket::SetReuseAddr(bool reuse) {
    int opt = static_cast<int>(reuse);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<char *>(&opt), sizeof(opt)) < 0) {
        THROW_SOCKET_ERROR("SetReuseAddr");
    }
}

void TcpSocket::Create(int af) {
    sockfd = socket(af, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        THROW_SOCKET_ERROR("Create");
    }
}

void TcpSocket::Bind(const SockAddr &addr, bool reuse) {
    SetReuseAddr(reuse);
    Socket::Bind(addr);
}

bool TcpSocket::Listen(int backlog) {
    if (listen(sockfd, backlog) == -1) {
        return false;
    } else {
        return true;
    }
}

TcpSocket TcpSocket::Accept() {
    SOCKET newfd = accept(sockfd, NULL, NULL);
    if (newfd == INVALID_SOCKET) {
        THROW_SOCKET_ERROR("Accept");
    }
    return TcpSocket(newfd, false);
}

int TcpSocket::AtMark() const {
#ifdef _WIN32
    unsigned long atmark;  // NOLINT(*)
    if (ioctlsocket(sockfd, SIOCATMARK, &atmark) != NO_THROW_SOCKET_ERROR)
        return -1;
#else
    int atmark;
    if (ioctl(sockfd, SIOCATMARK, &atmark) == -1) return -1;
#endif
    return static_cast<int>(atmark);
}

bool TcpSocket::Connect(const SockAddr &addr) {
    int ret = connect(sockfd, reinterpret_cast<const sockaddr *>(&addr.addr),
                      sizeof(addr.addr));
    if (ret != 0) {
        THROW_SOCKET_ERROR("Connect");
        return false;
    }
    return true;
}

bool TcpSocket::Connect(const std::string &url, int port) {
    SockAddr serv_addr(url, port);
    return Connect(serv_addr);
}

ssize_t TcpSocket::Send(const void *buf_, size_t len, int flag) {
    const char *buf = reinterpret_cast<const char *>(buf_);
    return send(sockfd, buf, static_cast<sock_size_t>(len), flag);
}

ssize_t TcpSocket::Recv(void *buf_, size_t len, int flags) {
    char *buf = reinterpret_cast<char *>(buf_);
    return recv(sockfd, buf, static_cast<sock_size_t>(len), flags);
}

size_t TcpSocket::SendAll(const void *buf_, size_t len) {
    const char *buf = reinterpret_cast<const char *>(buf_);
    size_t ndone = 0;
    while (ndone < len) {
        ssize_t ret = send(sockfd, buf, static_cast<ssize_t>(len - ndone), 0);
        if (ret == -1) {
            if (LastErrorWouldBlock()) return ndone;
            THROW_SOCKET_ERROR("SendAll");
        }
        buf += ret;
        ndone += ret;
    }
    return ndone;
}

size_t TcpSocket::RecvAll(void *buf_, size_t len) {
    char *buf = reinterpret_cast<char *>(buf_);
    size_t ndone = 0;
    while (ndone < len) {
        ssize_t ret = recv(sockfd, buf, static_cast<sock_size_t>(len - ndone),
                           MSG_WAITALL);
        if (ret == -1) {
            if (LastErrorWouldBlock()) return ndone;
            THROW_SOCKET_ERROR("RecvAll");
        }
        if (ret == 0) return ndone;
        buf += ret;
        ndone += ret;
    }
    return ndone;
}

void TcpSocket::SendStr(const std::string &str) {
    int32_t len = static_cast<int32_t>(str.length());
    CHECK_F(this->SendAll(&len, sizeof(len)) == sizeof(len),
            "error during send SendStr");
    if (len != 0) {
        CHECK_F(this->SendAll(str.c_str(), str.length()) == str.length(),
                "error during send SendStr");
    }
}

void TcpSocket::SendBytes(void *buf_, int32_t len) {
    CHECK_F(this->SendAll(&len, sizeof(len)) == sizeof(len),
            "error during send SendBytes");
    if (len != 0) {
        CHECK_F(this->SendAll(buf_, len) == len, "error during send SendBytes");
    }
}

void TcpSocket::RecvStr(std::string &out_str) {
    int32_t len;
    CHECK_F(this->RecvAll(&len, sizeof(len)) == sizeof(len),
            "error during send RecvStr");
    out_str.resize(len);
    if (len != 0) {
        CHECK_F(this->RecvAll(&(out_str)[0], len) == out_str.length(),
                "error during send SendStr");
    }
}

void TcpSocket::RecvBytes(void *buf_, int32_t &len) {
    CHECK_F(this->RecvAll(&len, sizeof(len)) == sizeof(len),
            "error during send RecvBytes");
    if (len != 0) {
        CHECK_F(this->RecvAll(buf_, len) == len, "error during send RecvBytes");
    }
}

void TcpSocket::SendInt(const int32_t &val) {
    CHECK_F(this->SendAll(&val, sizeof(val)) == sizeof(val),
            "error during send SendInt");
}

void TcpSocket::RecvInt(int32_t &out_val) {
    CHECK_F(this->RecvAll(&out_val, sizeof(out_val)) == sizeof(out_val),
            "error during send RecvInt");
}
}  // namespace rdc
