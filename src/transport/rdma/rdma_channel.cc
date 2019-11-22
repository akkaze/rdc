#ifdef RDC_USE_RDMA
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "common/env.h"
#include "core/logging.h"
#include "transport/rdma/rdma_adapter.h"
#include "transport/rdma/rdma_channel.h"
#include "transport/rdma/rdma_memory_mgr.h"
#include "transport/rdma/rdma_utils.h"
#include "transport/tcp/socket.h"
#include "utils/utils.h"

namespace rdc {

const uint64_t kMTU = 1 << 23;
static inline uint64_t div_up(uint64_t dividend, uint64_t divisor) {
    return (dividend + divisor - 1) / divisor;
}

RdmaChannel::RdmaChannel() : RdmaChannel(RdmaAdapter::Get()) {}

RdmaChannel::RdmaChannel(RdmaAdapter* adapter)
    : RdmaChannel(adapter, Env::Get()->GetEnv("RDC_RDMA_BUFSIZE", kBufSize)) {}
RdmaChannel::RdmaChannel(RdmaAdapter* adapter, uint64_t buf_size)
    : adapter_(adapter), buf_size_(buf_size) {
    send_buf_ = reinterpret_cast<uint8_t*>(utils::AllocTemp(buf_size_));
    recv_buf_ = reinterpret_cast<uint8_t*>(utils::AllocTemp(buf_size_));
    num_comp_queue_entries_ = adapter->max_num_queue_entries();
    InitRdmaContext();
}

RdmaChannel::~RdmaChannel() {
    utils::Free(send_buf_);
    utils::Free(recv_buf_);
}

void RdmaChannel::InitRdmaContext() {
    send_memory_region_ =
        ibv_reg_mr(adapter_->protection_domain(), send_buf_, buf_size_,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (send_memory_region_ == nullptr) {
        LOG_F(ERROR, "Fail to create sending memory region : %s",
              std::strerror(errno));
    }
    recv_memory_region_ =
        ibv_reg_mr(adapter_->protection_domain(), recv_buf_, buf_size_,
                   IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (recv_memory_region_ == nullptr) {
        LOG_F(ERROR, "Fail to create receiving memory region : %s",
              std::strerror(errno));
    }
    CreateQueuePair();
    CreateLocalAddr();
}

void RdmaChannel::ExitRdmaContext() {
    CHECK_EQ(ibv_destroy_qp(queue_pair_), 0);
    CHECK_EQ(ibv_dereg_mr(send_memory_region_), 0);
    CHECK_EQ(ibv_dereg_mr(recv_memory_region_), 0);
}
void RdmaChannel::SetQueuePairForReady() {
    InitQueuePair();
    EnableQueuePairForRecv();
    EnableQueuePairForSend();
}

Status RdmaChannel::Connect(const std::string& hostname, const uint32_t& port) {
    TcpSocket peer_sock;
    peer_sock.Connect(hostname, port);
    // send addr to peer
    peer_sock.SendStr(own_rdma_addr_.to_string());
    // recv peer addr
    std::string peer_rdma_addr_str;
    peer_sock.RecvStr(peer_rdma_addr_str);
    peer_rdma_addr_.from_string(peer_rdma_addr_str);
    SetQueuePairForReady();
    return Status::kSuccess;
}

void RdmaChannel::CreateQueuePair() {
    ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.send_cq = adapter_->completion_queue();
    qp_init_attr.recv_cq = adapter_->completion_queue();
    qp_init_attr.srq = adapter_->shared_receive_queue();
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.cap.max_send_wr =
        Env::Get()->GetEnv("RDC_RDMA_MAX_WR", kNumCompQueueEntries);
    qp_init_attr.cap.max_recv_wr =
        Env::Get()->GetEnv("RDC_RDMA_MAX_WR", kNumCompQueueEntries);
    qp_init_attr.cap.max_send_sge = 16;
    qp_init_attr.cap.max_recv_sge = 16;
    qp_init_attr.cap.max_inline_data = 1U << 6;
    queue_pair_ = ibv_create_qp(adapter_->protection_domain(), &qp_init_attr);
    CHECK_NOTNULL(queue_pair_);
    adapter_->set_ready(true);
}

void RdmaChannel::InitQueuePair() {
    ibv_qp_attr* attr = new ibv_qp_attr;
    memset(attr, 0, sizeof(*attr));

    attr->qp_state = IBV_QPS_INIT;
    attr->pkey_index = 0;
    attr->port_num = adapter_->ib_port();
    attr->qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;

    CHECK_EQ(ibv_modify_qp(queue_pair_, attr,
                           IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |
                               IBV_QP_ACCESS_FLAGS),
             0)
        << "Could not modify QP to INIT, ibv_modify_qp";
    delete attr;
}
void RdmaChannel::EnableQueuePairForRecv() {
    ibv_qp_attr* attr = new ibv_qp_attr;

    memset(attr, 0, sizeof(*attr));

    attr->qp_state = IBV_QPS_RTR;
    attr->path_mtu = IBV_MTU_4096;
    attr->dest_qp_num = peer_rdma_addr_.qpn;
    attr->rq_psn = peer_rdma_addr_.psn;
    attr->max_dest_rd_atomic = 1;
    attr->min_rnr_timer = 12;
    attr->ah_attr.is_global = 1;
    attr->ah_attr.dlid = peer_rdma_addr_.lid;
    attr->ah_attr.sl = 0;
    attr->ah_attr.src_path_bits = 0;
    attr->ah_attr.port_num = adapter_->ib_port();
    attr->ah_attr.grh.dgid.global.subnet_prefix = peer_rdma_addr_.snp;
    attr->ah_attr.grh.dgid.global.interface_id = peer_rdma_addr_.iid;
    attr->ah_attr.grh.sgid_index = adapter_->sgid_idx();
    attr->ah_attr.grh.flow_label = 0;
    attr->ah_attr.grh.hop_limit = 255;
    CHECK_EQ(
        ibv_modify_qp(queue_pair_, attr,
                      IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU |
                          IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
                          IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER),
        0)
        << "Could not modify QP to RTR state";

    delete attr;
}

void RdmaChannel::EnableQueuePairForSend() {
    ibv_qp_attr* attr = new ibv_qp_attr;
    memset(attr, 0, sizeof *attr);

    attr->qp_state = IBV_QPS_RTS;
    attr->timeout = 14;
    attr->retry_cnt = 7;
    attr->rnr_retry = 7; /* infinite retry */
    attr->sq_psn = own_rdma_addr_.psn;
    attr->max_rd_atomic = 1;

    CHECK_EQ(ibv_modify_qp(queue_pair_, attr,
                           IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                               IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN |
                               IBV_QP_MAX_QP_RD_ATOMIC),
             0)
        << "Could not modify QP to RTS state";
}

void RdmaChannel::CreateLocalAddr() {
    ibv_port_attr attr;
    ibv_query_port(adapter_->context(), adapter_->ib_port(), &attr);
    own_rdma_addr_.lid = attr.lid;
    own_rdma_addr_.qpn = queue_pair_->qp_num;
    own_rdma_addr_.psn = rand() & 0xffffff;
    own_rdma_addr_.snp = adapter_->snp();
    own_rdma_addr_.iid = adapter_->iid();
    own_rdma_addr_.rkey = recv_memory_region_->rkey;
    own_rdma_addr_.raddr = (uint64_t)recv_buf_;
}
WorkCompletion RdmaChannel::ISend(const Buffer& sendbuf) {
    ibv_mr* mr = nullptr;
    if (sendbuf.pinned()) {
        mr = sendbuf.memory_region();
    } else {
        mr = RdmaMemoryMgr::Get()->FindOrInsert(sendbuf.addr(),
                                                sendbuf.size_in_bytes());
    }
    RdmaChnnaelInfo channel_info;
    channel_info.buf_pinned = sendbuf.pinned();
    uint64_t req_id = WorkRequestManager::Get()->NewWorkRequest(
        kSend, sendbuf.addr(), sendbuf.size_in_bytes(), channel_info);

    auto num_parts = div_up(size, kMTU);
    std::vector<ibv_send_wr> send_wrs(num_parts);
    for (auto i = 0U; i < num_parts; i++) {
        std::memset(&send_wrs[i], 0, sizeof(ibv_send_wr));
        send_wrs[i].wr_id = req_id;
        send_wrs[i].num_sge = 1;
        send_wrs[i].opcode = IBV_WR_SEND;

        struct ibv_sge sge_list;
        std::memset(&sge_list, 0, sizeof(struct ibv_sge));
        sge_list.addr = (uint64_t)sendbuf.addr() + i * kMTU;
        sge_list.length = size - i * kMTU;
        sge_list.lkey = mr->lkey;

        send_wrs[i].sg_list = &sge_list;

        if (i == num_parts - 1) {
            send_wrs[i].send_flags = IBV_SEND_SIGNALED;
            send_wrs[i].next = nullptr;
        } else {
            send_wrs[i].next = &send_wrs[i + 1];
        }
    }
    //    send_wr.wr.rdma.rkey = peer_rdma_addr_.rkey;
    //    send_wr.wr.rdma.remote_addr = peer_rdma_addr_.raddr;
    //    send_wr.imm_data = 0;

    ibv_send_wr* bad_wr;
    auto rc = ibv_post_send(queue_pair_, &send_wrs[0], &bad_wr);
    if (rc != 0) {
        LOG_F(ERROR, "ibv_post_send failed: %s.", std::strerror(errno));
    }
    WorkCompletion wc(req_id);
    return wc;
}

WorkCompletion RdmaChannel::IRecv(void* recvbuf_, size_t size) {
    ibv_mr* mr = nullptr;
    if (recvbuf.pinned()) {
        mr = recvbuf.memory_region();
    } else {
        mr = RdmaMemoryMgr::Get()->FindOrInsert(recvbuf.addr(),
                                                recvbuf.size_in_bytes());
    }
    RdmaChnnaelInfo channel_info;
    channel_info.buf_pinned = recvbuf.pinned();
    uint64_t req_id = WorkRequestManager::Get()->NewWorkRequest(
        kSend, recvbuf.addr(), recvbuf.size_in_bytes(), channel_info);

    auto num_parts = div_up(size, kMTU);
    std::vector<ibv_recv_wr> recv_wrs(num_parts);
    for (auto i = 0U; i < num_parts; i++) {
        ibv_sge sge_list;
        std::memset(&sge_list, 0, sizeof(struct ibv_sge));
        sge_list.addr = (uint64_t)recvbuf_ + i * kMTU;
        sge_list.length = size - i * kMTU;
        sge_list.lkey = mr->lkey;

        std::memset(&recv_wrs[i], 0, sizeof(ibv_recv_wr));
        recv_wrs[i].wr_id = req_id;
        recv_wrs[i].sg_list = &sge_list;
        recv_wrs[i].num_sge = 1;
        if (i == num_parts - 1) {
            recv_wrs[i].next = nullptr;
        } else {
            recv_wrs[i].next = &recv_wrs[i + 1];
        }
    }
    ibv_recv_wr* bad_wr;
    if (adapter_->use_srq()) {
        auto rc = ibv_post_srq_recv(adapter_->shared_receive_queue(),
                                    &recv_wrs[0], &bad_wr);
        if (rc != 0) {
            LOG_F(ERROR, "ibv_post_srq_recv failed: %s.", std::strerror(errno));
        }
    } else {
        auto rc = ibv_post_recv(queue_pair_, &recv_wrs[0], &bad_wr);
        if (rc != 0) {
            LOG_F(ERROR, "ibv_post_recv failed %s.", std::strerror(errno));
        }
    }
    WorkCompletion wc(req_id);
    return wc;
}
}  // namespace rdc
#endif
