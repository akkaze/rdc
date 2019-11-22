#pragma once

#include <atomic>
#include "common/env.h"
#include "transport/tcp/socket.h"
#include "transport/adapter.h"
#include "transport/rdma/rdma_channel.h"

namespace rdc {
class RdmaAdapter : public IAdapter {
public:
    RdmaAdapter();
    static RdmaAdapter* Get() {
        static RdmaAdapter adapter;
        return &adapter;
    }
    ~RdmaAdapter();

    void PollForever();

    int Listen(const uint32_t& tcp_port);
    IChannel* Accept();

    uint8_t ib_port() const { return ib_port_; }
    ibv_context* context() const { return context_; }
    ibv_pd* protection_domain() const { return protection_domain_; }

    ibv_mtu mtu() const { return mtu_; }

    ibv_cq* completion_queue() const { return completion_queue_; }
    // atomic operations
    bool ready() const { return ready_.load(std::memory_order_acquire); }
    void set_ready(const bool& ready) {
        ready_.store(ready, std::memory_order_release);
    }
    bool finished() const { return finished_.load(std::memory_order_acquire); }
    void set_finished(const bool& finished) {
        finished_.store(finished, std::memory_order_release);
    }

    ibv_gid gid() const { return gid_; }
    int sgid_idx() const { return sgid_idx_; }
    uint32_t snp() { return snp_; }
    uint64_t iid() { return iid_; }
    uint64_t max_num_queue_entries() { return dev_attr_.max_cqe; }
    ibv_srq* shared_receive_queue() { return shared_receive_queue_; }
    bool use_srq() const { return use_srq_; }

protected:
    void InitRdmaContext();
    void ExitRdmaContext();

private:
    std::unique_ptr<TcpSocket> listen_sock_;
    uint32_t timeout_;
    // device related
    ibv_device* dev_;
    uint8_t ib_port_;
    ibv_mtu mtu_;
    ibv_device_attr dev_attr_;
    ibv_context* context_;
    ibv_pd* protection_domain_;
    // comptetion queue related
    ibv_cq* completion_queue_;
    ibv_comp_channel* event_channel_;
    uint8_t pkey_index_;
    uint8_t sl_;
    uint8_t traffic_class_;
    uint32_t retry_count_;
    // srq related
    bool use_srq_;
    ibv_srq* shared_receive_queue_;

    std::atomic<bool> ready_;
    std::atomic<bool> finished_;
    std::unique_ptr<std::thread> poll_thread_;
    // shared by all rdma address
    ibv_gid gid_;
    uint8_t sgid_idx_;
    uint8_t gid_idx_;
    uint32_t snp_;
    uint64_t iid_;
};
}  // namespace rdc
