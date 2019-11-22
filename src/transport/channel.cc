#include "transport/channel.h"
namespace rdc {
std::string ChannelKindToString(const ChannelKind& channel_kind) {
    switch (channel_kind) {
        case ChannelKind::kRead:
            return "read";
        case ChannelKind::kWrite:
            return "write";
        case ChannelKind::kReadWrite:
            return "readwrite";
        case ChannelKind::kNone:
            return "none";
    }
}

IChannel::IChannel(const ChannelKind& kind)
    : kind_(kind), error_detected_(false) {
}

bool IChannel::Connect(const std::string& addr_str) {
    Backend backend;
    std::string host;
    uint32_t port;
    std::tie(backend, host, port) = ParseAddr(addr_str);
    return Connect(host, port);
}

WorkCompletion* IChannel::ISend(const void* sendaddr,
                                const uint64_t& sendbytes) {
    Buffer sendbuf(sendaddr, sendbytes);
    return this->ISend(sendbuf);
}

WorkCompletion* IChannel::IRecv(void* recvaddr, const uint64_t& recvbytes) {
    Buffer recvbuf(recvaddr, recvbytes);
    return this->IRecv(recvbuf);
}

WorkStatus IChannel::SendInt(int32_t val) {
    auto wc = this->ISend(&val, sizeof(int32_t));
    wc->Wait();
    auto status = wc->status();
    WorkCompletion::Delete(wc);
    return status;
}

WorkStatus IChannel::SendStr(std::string str) {
    int32_t size = static_cast<int32_t>(str.size());
    auto chain_wc = ChainWorkCompletion::New();
    auto wc = this->ISend(&size, sizeof(size));
    chain_wc->Add(wc);
    wc = this->ISend(utils::BeginPtr(str), str.size());
    chain_wc->Add(wc);
    chain_wc->Wait();
    auto status = chain_wc->status();
    ChainWorkCompletion::Delete(chain_wc);
    return status;
}

WorkStatus IChannel::SendBytes(void* ptr, const int32_t& sendbytes) {
    auto chain_wc = ChainWorkCompletion::New();
    auto wc = this->ISend(&sendbytes, sizeof(sendbytes));
    chain_wc->Add(wc);
    wc = this->ISend(ptr, sendbytes);
    chain_wc->Add(wc);
    chain_wc->Wait();
    auto status = chain_wc->status();
    ChainWorkCompletion::Delete(chain_wc);
    return status;
}

WorkStatus IChannel::RecvInt(int32_t& val) {
    auto wc = this->IRecv(&val, sizeof(int32_t));
    wc->Wait();
    auto status = wc->status();
    WorkCompletion::Delete(wc);
    return status;
}

WorkStatus IChannel::RecvStr(std::string& str) {
    int32_t size = 0;
    auto wc = this->IRecv(&size, sizeof(int32_t));
    wc->Wait();
    auto status = wc->status();
    if (status != WorkStatus::kFinished) {
        WorkCompletion::Delete(wc);
        return status;
    }
    str.resize(size);
    wc = this->IRecv(utils::BeginPtr(str), str.size());
    status = wc->status();
    WorkCompletion::Delete(wc);
    return status;
}

WorkStatus IChannel::RecvBytes(void* ptr, int32_t& recvbytes) {
    auto wc = this->IRecv(&recvbytes, sizeof(int32_t));
    wc->Wait();
    auto status = wc->status();
    if (status != WorkStatus::kFinished) {
        WorkCompletion::Delete(wc);
        return status;
    }
    wc = this->IRecv(ptr, recvbytes);
    status = wc->status();
    WorkCompletion::Delete(wc);
    return status;
}

bool IChannel::CanRead() const {
    return kind_ == ChannelKind::kRead || kind_ == ChannelKind::kReadWrite;
}

bool IChannel::CanWrite() const {
    return kind_ == ChannelKind::kWrite || kind_ == ChannelKind::kReadWrite;
}

bool IChannel::CheckError() const {
    return error_detected_.load(std::memory_order_acquire);
}
//---------------------------properties--------------------------------
void IChannel::set_error_detected(const bool& error_detected) {
    error_detected_.store(error_detected, std::memory_order_release);
}

bool IChannel::error_detected() const {
    return error_detected_.load(std::memory_order_acquire);
}

ChannelKind IChannel::kind() const {
    return kind_;
}

void IChannel::set_kind(const ChannelKind& kind) {
    kind_ = kind;
}
}  // namespace rdc
