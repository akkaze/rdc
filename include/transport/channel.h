#pragma once
#include <string>
#include "common/status.h"
#include "core/work_request.h"
#include "transport/adapter.h"
#include "transport/buffer.h"
namespace rdc {
const uint32_t kCommTimeoutMs = 600;

enum class ChannelKind : uint32_t {
    kRead,
    kWrite,
    kReadWrite,
    kNone,
};

std::string ChannelKindToString(const ChannelKind& channel_kind);

class IChannel {
public:
    IChannel() = default;
    IChannel(const ChannelKind& kind);

    virtual ~IChannel() = default;

    virtual WorkCompletion* ISend(Buffer sendbuf) = 0;

    virtual WorkCompletion* IRecv(Buffer recvbuf) = 0;

    virtual void Close() = 0;

    virtual bool Connect(const std::string& host, const uint32_t& port) = 0;

    bool Connect(const std::string& addr_str);

    WorkCompletion* ISend(const void* sendaddr, const uint64_t& sendbytes);

    WorkCompletion* IRecv(void* recvaddr, const uint64_t& recvbytes);

    WorkStatus SendInt(int32_t val);

    WorkStatus SendStr(std::string str);

    WorkStatus RecvInt(int32_t& val);

    WorkStatus RecvStr(std::string& str);

    WorkStatus SendBytes(void* ptr, const int32_t& sendbytes);

    WorkStatus RecvBytes(void* ptr, int32_t& recvbytes);

    bool CheckError() const;

    bool CanRead() const;

    bool CanWrite() const;
    //---------------------------properties--------------------------------
    void set_error_detected(const bool& error_detected);

    bool error_detected() const;

    ChannelKind kind() const;

    void set_kind(const ChannelKind& kind);

    std::string comm() const {
        return comm_;
    }

    void set_comm(const std::string& comm) {
        comm_ = comm;
    }

    int peer_rank() const {
        return peer_rank_;
    }

    void set_peer_rank(const int& peer_rank) {
        peer_rank_ = peer_rank;
    }

private:
    ChannelKind kind_;
    std::atomic<bool> error_detected_;
    std::string comm_ = "null";
    int peer_rank_ = -1;
};
}  // namespace rdc
