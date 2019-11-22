#pragma once
#include <infiniband/verbs.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common/status.h"
#include "core/work_request.h"
#include "transport/buffer.h"
#include "transport/channel.h"
#include "utils/utils.h"

namespace rdc {
const uint64_t kBufSize = 1U << 12;
const int kNumCompQueueEntries = 1024;

struct RdmaAddr {
    uint32_t lid;
    uint32_t qpn;
    uint32_t psn;
    uint64_t snp;
    uint64_t iid;
    uint32_t rkey;
    uint64_t raddr;

    std::string to_string() {
        auto addr_str = str_utils::SPrintf("%d:%d:%d:%ld:%ld:%d:%ld", this->lid,
                                           this->qpn, this->psn, this->snp,
                                           this->iid, this->rkey, this->raddr);
        return addr_str;
    }

    void from_string(const std::string& addr_str) {
        str_utils::SScanf(addr_str, "%d:%d:%d:%ld:%ld:%d:%ld", &this->lid,
                          &this->qpn, &this->psn, &this->snp, &this->iid,
                          &this->rkey, &this->raddr);
    }
};

struct RdmaChannelInfo {
    bool buf_pinned;
};

class RdmaAdapter;
class RdmaChannel : public IChannel {
public:
    RdmaChannel();
    RdmaChannel(RdmaAdapter* adapter, uint64_t buf_size);
    RdmaChannel(RdmaAdapter* adapter);
    ~RdmaChannel();
    WorkCompletion ISend(const Buffer& sendbuf) override;
    WorkCompletion IRecv(Buffer& recvbuf) override;
    Status Connect(const std::string& hostname, const uint32_t& port) override;
    void SetQueuePairForReady();
    void Close() override { return this->ExitRdmaContext(); }

    void set_own_rdma_addr(const RdmaAddr& rdma_addr) {
        own_rdma_addr_ = rdma_addr;
    }

    RdmaAddr own_rdma_addr() const { return own_rdma_addr_; }

    void set_peer_rdma_addr(const RdmaAddr& peer_rdma_addr) {
        peer_rdma_addr_ = peer_rdma_addr;
    }

    RdmaAddr peer_rdma_addr() const { return peer_rdma_addr_; }

protected:
    void InitRdmaContext();
    void ExitRdmaContext();
    void CreateQueuePair();
    void CreateLocalAddr();
    void InitQueuePair();
    void EnableQueuePairForSend();
    void EnableQueuePairForRecv();

private:
    RdmaAdapter* adapter_;
    uint8_t* send_buf_;
    uint8_t* recv_buf_;
    uint64_t buf_size_;

    RdmaAddr own_rdma_addr_;
    RdmaAddr peer_rdma_addr_;

    ibv_qp* queue_pair_;
    ibv_mr* send_memory_region_;
    ibv_mr* recv_memory_region_;
    int num_comp_queue_entries_;
};
}  // namespace rdc
