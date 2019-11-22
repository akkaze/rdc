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
#include "core/logging.h"
#if defined(_WIN32)
typedef int ssize_t;
typedef int sock_size_t;
#else
typedef int SOCKET;
typedef size_t sock_size_t;
const int INVALID_SOCKET = -1;
#endif
namespace rdc {
/*! @brief data structure for network address */
struct SockAddr {
    sockaddr_in addr;
    // constructor
    SockAddr();
    SockAddr(const char *url, int port);
    SockAddr(std::string host, int port);
    static std::string GetHostName();
    /*!
     * @brief set the address
     * @param url the url of the address
     * @param port the port of address
     */
    void Set(const char *host, int port);
    /*! @brief return port of the address*/
    int port() const;
    /*! @return a string representation of the address */
    std::string AddrStr() const;
}; 

/*!
 * @brief base class containing common operations of Tcp and Udp sockets
 */
class Socket {
public:
    /*! @brief the file descriptor of socket */
    SOCKET sockfd;
    // default conversion to int
    operator SOCKET() const {
        return sockfd;
    }
    /*!
     * @return last error of socket operation
     */
    static int GetLastError();

    /*! @return whether last error was would block */
    bool LastErrorWouldBlock();
    /*!
     * @brief start up the socket module
     *   call this before using the sockets
     */
    static void Startup();
    /*!
     * @brief shutdown the socket module after use, all sockets need to be
     * closed
     */
    static void Finalize();
    /*!
     * @brief set this socket to use non-blocking mode
     * @param non_block whether set it to be non-block, if it is false
     *        it will set it back to block mode
     */
    void SetNonBlock(bool non_block);
    /*!
     * @brief bind the socket to an address
     * @param addr
     * @param reuse whether or not to resue the address
     */
    void Bind(const SockAddr &addr);
    /*!
     * @brief try bind the socket to host, from start_port to end_port
     * @param end_port ending port number to try
     * @return the port successfully bind to, return -1 if failed to bind any
     * port
     */
    int TryBindHost(int port);
    /*!
     * @brief try bind the socket to host, from start_port to end_port
     * @param start_port starting port number to try
     * @param end_port ending port number to try
     * @return the port successfully bind to, return -1 if failed to bind any
     * port
     */
    int TryBindHost(int start_port, int end_port);
    /*! @brief get last error code if any */
    int GetSockError() const;
    /*! @brief check if anything bad happens */
    bool BadSocket() const;
    /*! @brief check if socket is already closed */
    bool IsClosed() const;
    /*! @brief close the socket */
    void Close();
    // report an socket error
    static void Error(const char *msg);

protected:
    explicit Socket(SOCKET sockfd);
};

/*!
 * @brief a wrapper of Tcp socket that hopefully be cross platform
 */
class TcpSocket : public Socket {
public:
    // constructor
    TcpSocket(bool create = true);
    explicit TcpSocket(SOCKET sockfd, bool create = true);
    /*!
     * @brief enable/disable Tcp keepalive
     * @param keepalive whether to set the keep alive option on
     */
    void SetKeepAlive(bool keepalive);
    /*!
     * @brief enable/disable Tcp addr_reuse
     * @param reuse whether to set the address reuse option on
     */
    void SetReuseAddr(bool reuse);
    /*!
     * @brief create the socket, call this before using socket
     * @param af domain
     */
    void Create(int af = PF_INET);
    /*!
     * @brief bind the socket to an address
     * @param addr
     * @param reuse whether or not to resue the address
     */
    void Bind(const SockAddr &addr, bool reuse = true);

    /*!
     * @brief perform listen of the socket
     * @param backlog backlog parameter
     */
    bool Listen(int backlog = 128);
    /*! @brief get a new connection */
    TcpSocket Accept();
    /*!
     * @brief decide whether the socket is at OOB mark
     * @return 1 if at mark, 0 if not, -1 if an error occured
     */
    int AtMark() const;
    /*!
     * @brief connect to an address
     * @param addr the address to connect to
     * @return whether connect is successful
     */
    bool Connect(const SockAddr &addr);
    /*!
     * @brief connect to an address
     * @param url the hostname of the server address
     * @param port the port of the server address
     * @return whether connect is successful
     */
    bool Connect(const std::string &url, int port);
    /*!
     * @brief send data using the socket
     * @param buf the pointer to the buffer
     * @param len the size of the buffer
     * @param flags extra flags
     * @return size of data actually sent
     *         return -1 if error occurs
     */
    ssize_t Send(const void *buf_, size_t len, int flag = 0);
    /*!
     * @brief receive data using the socket
     * @param buf_ the pointer to the buffer
     * @param len the size of the buffer
     * @param flags extra flags
     * @return size of data actually received
     *         return -1 if error occurs
     */
    ssize_t Recv(void *buf_, size_t len, int flags = 0);
    /*!
     * @brief peform block write that will attempt to send all data out
     *    can still return smaller than request when error occurs
     * @param buf the pointer to the buffer
     * @param len the size of the buffer
     * @return size of data actually sent
     */
    size_t SendAll(const void *buf_, size_t len);
    /*!
     * @brief peforma block read that will attempt to read all data
     *    can still return smaller than request when error occurs
     * @param buf_ the buffer pointer
     * @param len length of data to recv
     * @return size of data actually sent
     */
    size_t RecvAll(void *buf_, size_t len);
    /*!
     * @brief send a string over network
     * @param str the string to be sent
     */
    void SendStr(const std::string &str);
    void SendBytes(void* buf_, int32_t len);
    /*!
     * @brief recv a string from network
     * @param out_str the string to receive
     */
    void RecvStr(std::string &out_str);
    void RecvBytes(void* buf_, int32_t& len);
    /*!
     * @brief send a string over network
     * @param val the integer to be sent
     */
    void SendInt(const int32_t &val);
    /*!
     * @brief recv a int from network
     * @param out_val the integer to receive
     */
    void RecvInt(int32_t &out_val);
};
}  // namespace rdc
