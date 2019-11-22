#pragma once
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>


const int32_t kInvalidSocket = -1;
inline int GetLastSocketError(const int32_t& fd) {
    int  error = 0;
    socklen_t errlen = sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR,
        (void *)&error, &errlen) == 0) {
        return error;
    }
    return -1;
}

inline void CloseSocket(int32_t& fd) {
    if (fd != kInvalidSocket) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
    }
    fd = kInvalidSocket;
}
