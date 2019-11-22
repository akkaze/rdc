#ifdef RDC_USE_RDMA
#include "transport/rdma/rdma_adapter.h"
#include "core/logging.h"
#include "core/threadpool.h"
#include "transport/rdma/rdma_memory_mgr.h"
#include "transport/rdma/rdma_utils.h"

namespace rdc {

static const uint32_t kConcurrentOps = 128;
RdmaAdapter::RdmaAdapter() {
    this->set_backend(kRdma);
    InitRdmaContext();
    this->listen_sock_.reset(new TcpSocket);
    this->set_ready(false);
    this->set_finished(false);
    poll_thread_ = utils::make_unique<std::thread>([this] { PollForever(); });
}
RdmaAdapter::~RdmaAdapter() {
    this->set_finished(true);
    this->listen_sock_->Close();
    poll_thread_->join();
    ExitRdmaContext();
}

void RdmaAdapter::InitRdmaContext() {
    GetAvaliableDeviceAndPort(dev_, ib_port_);
    CHECK_NOTNULL(context_ = ibv_open_device(dev_));
    CHECK_NOTNULL(protection_domain_ = ibv_alloc_pd(context_));

    mtu_ = set_mtu(context_, ib_port_);

    auto rc = ibv_query_device(context_, &dev_attr_);
    CHECK_EQ_F(rc, 0, "Failed to query device");

    CHECK_NOTNULL(event_channel_ = ibv_create_comp_channel(context_));

    CHECK_NOTNULL(completion_queue_ =
                      ibv_create_cq(context_, kNumCompQueueEntries, context_,
                                    event_channel_, 0));
    // notify at creation
    CHECK_F(!ibv_req_notify_cq(completion_queue_, 0),
            "Failed to request CQ notification");
    // shared queue related
    if (Env::Get()->GetEnv("RDC_RDMA_USE_SRQ", 1)) {
        use_srq_ = true;
        ibv_srq_init_attr sqa;
        std::memset(&sqa, 0, sizeof(ibv_srq_init_attr));
        sqa.srq_context = this->context_;
        sqa.attr.max_wr =
            Env::Get()->GetEnv("RDC_RDMA_MAX_WR", kNumCompQueueEntries);
        sqa.attr.max_sge = 16;
        this->shared_receive_queue_ =
            ibv_create_srq(this->protection_domain_, &sqa);
        CHECK_NOTNULL(shared_receive_queue_);
    } else {
        use_srq_ = false;
    }
    // global addr info
    sgid_idx_ = roce::GetGid(ib_port_, context_);
    rc = ibv_query_gid(context_, ib_port_, gid_idx_, &gid_);
    CHECK_EQ_F(rc, 0, "Failed to query gid");

    snp_ = gid_.global.subnet_prefix;
    iid_ = gid_.global.interface_id;

    srand(time(0));
}

void RdmaAdapter::ExitRdmaContext() {
    CHECK_EQ(ibv_destroy_cq(completion_queue_), 0);
    CHECK_EQ(ibv_dealloc_pd(protection_domain_), 0);
    CHECK_EQ(ibv_close_device(context_), 0);
}
void RdmaAdapter::PollForever() {
    while (!ready()) {
    }
    for (;;) {
        if (finished()) {
            return;
        }
        ibv_cq* cq;
        void* cq_context;
        CHECK(!ibv_get_cq_event(event_channel_, &cq, &cq_context));
        CHECK(cq == completion_queue_);
        ibv_ack_cq_events(cq, 1);
        CHECK(!ibv_req_notify_cq(completion_queue_, 0));

        ibv_wc wcs[kConcurrentOps];
        int num_entries = 0;
        num_entries = ibv_poll_cq(completion_queue_, kConcurrentOps, wcs);
        CHECK_GE_F(num_entries, 0, "poll CQ failed");
        for (auto i = 0; i < num_entries; i++) {
            auto wc = wcs[i];
            CHECK_EQ_F(wc.status, IBV_WC_SUCCESS, "%s %d",
                       ibv_wc_status_str(wc.status), wc.status);
            auto& work_req =
                WorkRequestManager::Get()->GetWorkRequest(wc.wr_id);
            LOG(INFO) << GetRank() << " : " << wc.wr_id << " : " << num_entries;
            size_t len = 0;
            if (work_req.work_type() == kRecv) {
                len = wc.byte_len;
            } else {
                len = work_req.nbytes();
            }
            if (work_req.AddBytes(len)) {
                auto channel_info =
                    work_req.template extra_data<RdmaChannelInfo>();
                if (channel_info.buf_pinned) {
                    RdmaMemoryMgr::Get()->RemoveMemoryRegion(work_req.ptr(),
                                                             work_req.nbytes());
                }
                work_req.Notify();
            }
        }
    }
}

int RdmaAdapter::Listen(const uint32_t& tcp_port) {
    listen_sock_->TryBindHost(tcp_port);
    listen_sock_->Listen();
    return 0;
}

IChannel* RdmaAdapter::Accept() {
    // accept the connection$
    TcpSocket incoming_sock = listen_sock_->Accept();

    auto channel = new RdmaChannel(this);

    // recv addr from peer
    RdmaAddr peer_rdma_addr;
    std::string peer_rdma_addr_str;
    incoming_sock.RecvStr(peer_rdma_addr_str);
    peer_rdma_addr.from_string(peer_rdma_addr_str);
    channel->set_peer_rdma_addr(peer_rdma_addr);
    // send addr to peer
    auto owned_rdma_addr = channel->own_rdma_addr();
    incoming_sock.SendStr(owned_rdma_addr.to_string());

    channel->SetQueuePairForReady();
    incoming_sock.Close();
    return channel;
}
}  // namespace rdc
#endif
