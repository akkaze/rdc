#pragma once
#include <unistd.h>
#include <atomic>
#include "common/threadsafe_queue.h"
#include "core/work_request.h"
#include "transport/channel.h"
#include "transport/tcp/socket.h"
#include "transport/tcp/socket_utils.h"
namespace rdc {
class TcpAdapter;
/**
 * @brief: a channel which send and recv data on tcp protocal and ethernet
 */
class TcpChannel final : public IChannel {
public:
    TcpChannel();
    TcpChannel(const ChannelKind& kind);
    TcpChannel(TcpAdapter* adapter, const ChannelKind& kind);
    TcpChannel(TcpAdapter* adapter, const int& sockfd, const ChannelKind& kind);
    TcpChannel(TcpAdapter* adapter, const TcpSocket& sock,
               const ChannelKind& kind);
    virtual ~TcpChannel() override;
    bool Connect(const std::string& hostname, const uint32_t& port) override;
    WorkCompletion* ISend(Buffer sendbuf) override;
    WorkCompletion* IRecv(Buffer recvbuf) override;

    void Close() override;

    void ReadCallback();
    void WriteCallback();

    void AddEventOfInterest(const ChannelKind& kind);
    void DeleteEventOfInterest(const ChannelKind& kind);
    void ModifyKind(const ChannelKind& kind);
    int sockfd() const {
        return SOCKET(sock_);
    }

    void set_adapter(TcpAdapter* adapter) {
        adapter_ = adapter;
    }

    int fd() const {
        return sock_.sockfd;
    }
private:
    TcpSocket sock_;
    // send recv request queue
    ThreadsafeQueue<uint64_t> send_reqs_;
    ThreadsafeQueue<uint64_t> recv_reqs_;
    /** only used to enable accept and listen callbacks */
    TcpAdapter* adapter_;
    utils::SpinLock mu_;
    std::atomic<bool> closing_{false};
};
}  // namespace rdc
