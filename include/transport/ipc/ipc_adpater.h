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
 * @class IpcAdapter
 * @brief tcpadapther which will govern all tcp connections
 */
class IpcAdapter : public IAdapter {
public:
    IpcAdapter();
    static IpcAdapter* Get() {
        static IpcAdapter poller;
        return &poller;
    }
    std::unordered_map<int, IpcChannel*> channels_;
    ~IpcAdapter();

    void AddChannel(int fd, IpcChannel* channel);

    void AddChannel(IpcChannel* channel);

    void RemoveChannel(IpcChannel* channel);

    void Shutdown();

    void Listen(const int& port) override;

    IChannel* Accept() override;

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
    
    std::atomic<bool> shutdown_;
    std::atomic<bool> shutdown_called_;
    std::mutex shutdown_lock_;
};

}  // namespace rdc
