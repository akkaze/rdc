#pragma once
#include <unistd.h>
#include <atomic>
#include "common/threadsafe_queue.h"
#include "core/work_request.h"
#include "transport/channel.h"

namespace rdc {
const int kMemFileNameSize = 8;
class IpcAdapter;
/**
 * @brief: a channel which send and recv data on tcp protocal and ethernet
 */
class IpcChannel final : public IChannel {
public:
    IpcChannel();
    IpcChannel(IpcAdapter* adapter);
    virtual ~IpcChannel() override;
    bool Connect(const std::string& hostname, const uint32_t& port) override;
    WorkCompletion* ISend(Buffer sendbuf) override;
    WorkCompletion* IRecv(Buffer recvbuf) override;

    void Close() override;

    void set_adapter(IpcAdapter* adapter) {
        adapter_ = adapter;
    }

private:
    int send_counter_;
    int recv_counter_;
    /** only used to enable accept and listen callbacks */
    IpcAdapter* adapter_;
};
}  // namespace rdc
