#pragma once
#include <algorithm>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>
#include "transport/adapter.h"
#include "transport/channel.h"
#include "transport/tcp/tcp_channel.h"
#include "utils/lock_utils.h"

namespace rdc {
/**
 * @class TcpAdapter
 * @brief tcpadapther which will govern all tcp connections
 */
class TcpAdapter : public IAdapter {
public:
    TcpAdapter();
    static TcpAdapter* Get() {
        static TcpAdapter poller;
        return &poller;
    }
    std::unordered_map<int, TcpChannel*> channels_;
    ~TcpAdapter();

    void AddChannel(int fd, TcpChannel* channel);

    void AddChannel(TcpChannel* channel);

    void RemoveChannel(TcpChannel* channel);

    void ModifyChannel(TcpChannel* channel, const ChannelKind& target_kind);

    void Shutdown();

    void PollForever();

    bool Poll();

    void Listen(const int& port);

    IChannel* Accept() override;

    int32_t epoll_fd() const {
        return epoll_fd_;
    }

    bool shutdown() const {
        return shutdown_.load(std::memory_order_acquire);
    }
    void set_shutdown(const bool& shutdown) {
        shutdown_.store(shutdown, std::memory_order_release);
    }
    bool shutdown_called() const {
        return shutdown_called_.load(std::memory_order_acquire);
    }
    void set_shutdown_called(const bool& shutdown_called) {
        shutdown_called_.store(shutdown_called, std::memory_order_release);
    }

private:
    /** timeout duration */
    int32_t timeout_;
    /** epoll file descriptor*/
    int32_t epoll_fd_;
    int32_t shutdown_fd_;
    TcpSocket listen_sock_;

    std::atomic<bool> shutdown_;
    std::atomic<bool> shutdown_called_;
    // utils::SpinLock lock_;
    std::mutex lock_;
    std::mutex shutdown_lock_;
    std::unique_ptr<std::thread> loop_thrd;
    std::unique_ptr<std::thread> listen_thrd;
};

}  // namespace rdc
