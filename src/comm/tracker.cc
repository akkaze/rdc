#include "comm/tracker.h"
#include <thread>
#include "comm/communicator_manager.h"
#include "common/env.h"
#include "core/exception.h"
#include "sys/network.h"
#include "transport/adapter.h"
#if RDC_USE_SHMEM
#include "transport/ipc/ipc_adapter.h"
#endif
#include "utils/string_utils.h"
namespace rdc {
namespace comm {

std::atomic<Tracker*> Tracker::instance(nullptr);
std::mutex Tracker::create_mutex;
std::atomic<bool> Tracker::created(false);

Tracker::Tracker() {
    rank_ = -1;
    world_size_ = -1;
    tracker_uri_ = "NULL";
    tracker_port_ = 9000;
    tracker_connected_.store(false);
    tracker_closed_ = false;
    tracker_lock_ = std::make_shared<std::mutex>();
    worker_port_ = 9910;
    host_uri_ = "";
    connect_retry_ = 5;
}

Tracker::Tracker(const std::string& tracker_uri, const int& tracker_port)
    : Tracker() {
    tracker_uri_ = tracker_uri;
    tracker_port_ = tracker_port;
    bool restart = CommunicatorManager::Get()->restart();
    if (restart) {
        LOG_F(INFO, "Trying to restart cluster as new node");
        this->Connect("restart");
    } else {
        LOG_F(INFO, "Trying to start a new cluster");
        this->Connect("start");
    }
}

Tracker::~Tracker() {
    if (!this->tracker_closed_.load(std::memory_order_acquire)) {
        Lock();
        this->tracker_closed_.store(true, std::memory_order_release);
        this->tracker_sock_->Close();
        UnLock();
    }
}

Tracker* Tracker::Get() {
    bool created_ = created.load(std::memory_order_acquire);
    Tracker* instance_ = instance.load(std::memory_order_relaxed);
    if (created_ == false) {
        std::lock_guard<std::mutex> lock(create_mutex);
        created_ = created.load(std::memory_order_relaxed);
        // instance_ = instance.load(std::memory_order_acquire);
        if (created_ == false) {
            auto&& tracker_uri = CommunicatorManager::Get()->tracker_uri();
            auto&& tracker_port = CommunicatorManager::Get()->tracker_port();
            instance_ = new Tracker(tracker_uri, tracker_port);
            instance.store(instance_, std::memory_order_relaxed);
            created.store(true, std::memory_order_release);
        }
    }
    return instance_;
}

void Tracker::Release() {
    bool created_ = false;
    while ((created_ = created.load(std::memory_order_acquire)) == false) {
        continue;
    }
    Tracker* instance_ = instance.load(std::memory_order_relaxed);
    delete instance_;
    instance.store(nullptr, std::memory_order_relaxed);
}

void Tracker::Lock() const {
    tracker_lock_->lock();
}

void Tracker::UnLock() const {
    tracker_lock_->unlock();
}

void Tracker::SendInt(const int32_t& value) {
    tracker_sock_->SendInt(value);
}

void Tracker::SendStr(const std::string& str) {
    tracker_sock_->SendStr(str);
}

void Tracker::SendBytes(void* buf, int32_t size) {
    tracker_sock_->SendBytes(buf, size);
}

void Tracker::RecvInt(int32_t& value) {
    tracker_sock_->RecvInt(value);
}

void Tracker::RecvStr(std::string& str) {
    tracker_sock_->RecvStr(str);
}

void Tracker::RecvBytes(void* buf, int32_t& size) {
    tracker_sock_->RecvBytes(buf, size);
}

std::tuple<int, int> Tracker::Connect(const char* cmd) {
    tracker_lock_->lock();
    if (!tracker_connected()) {
        std::string interface, ip;
        network::GetAvailableInterfaceAndIP(&interface, &ip);
        worker_port_ = network::GetAvailablePort();
        LOG_F(INFO, "Binding on port %d", worker_port_);
#if RDC_USE_SHMEM
        shmem_worker_port_ = network::GetAvailablePort();
        LOG_F(INFO, "Binding on port %d", shmem_worker_port_);
#endif
        this->host_uri_ = ip;
        // get information from tracker

        LOG_F(INFO, "Trying to connect to tracker at: [%s:%d]\n",
              tracker_uri_.c_str(), tracker_port_);
        tracker_sock_ = std::make_shared<TcpSocket>();
        int retry = 0;
        try {
            do {
                if (!tracker_sock_->Connect(tracker_uri_.c_str(),
                                            tracker_port_)) {
                    if (++retry >= connect_retry_) {
                        LOG_F(ERROR, "Connect to (failed): [%s:%d]\n",
                              tracker_uri_.c_str(), tracker_port_);
                        LOG_F(ERROR, "Connect");
                    } else {
                        LOG_F(ERROR,
                              "Retry connect to ip(retry time %d): [%s:%d]\n",
                              retry, tracker_uri_.c_str(), tracker_port_);
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        continue;
                    }
                }
                break;
            } while (true);
        } catch (Exception& exc) {
            PrintException(exc);
        }
        this->set_tracker_connected(true);
        // start listener at very begining
        GetAdapter()->Listen(worker_port_);
#if RDC_USE_SHMEM
        LOG_F(INFO, "Listening using IPC adapter");
        IpcAdapter::Get()->Listen(shmem_worker_port_);
#endif
    }
    tracker_sock_->SendStr(std::string(cmd));
    rank_ = Env::Get()->GetEnv("RDC_RANK", -1);
    // first send my rank to tracker for global rank scheduling
    tracker_sock_->SendInt(rank_);
    if (std::string(cmd) == "restart") {
        num_pending_nodes_ = Env::Get()->GetIntEnv("RDC_PENDING_NODES");
        tracker_sock_->SendInt(num_pending_nodes_);
    }
    // send my addr to tracker for decision making
    auto backend_str = GetAdapter()->backend_str();
    auto host_addr = str_utils::SPrintf("%s:%s:%d", backend_str.c_str(),
                                        host_uri_.c_str(), worker_port_);
    tracker_sock_->SendStr(host_addr);
#if RDC_USE_SHMEM
    auto shmem_host_addr =
        str_utils::SPrintf("ipc:%s:%d", host_uri_.c_str(), shmem_worker_port_);
    tracker_sock_->SendStr(shmem_host_addr);
#endif

    tracker_sock_->RecvInt(num_dead_nodes_);
    if (num_dead_nodes_ > 0) {
        dead_nodes_.clear();
        dead_nodes_.resize(num_dead_nodes_);
        for (auto&& d = 0; d < num_dead_nodes_; d++) {
            tracker_sock_->RecvInt(dead_nodes_[d]);
        }
    }
    tracker_sock_->RecvInt(num_pending_nodes_);
    LOG_F(INFO, "Number of pending nodes : %d", num_pending_nodes_);

    int32_t num_peers_with_same_host = 0;
    tracker_sock_->RecvInt(num_peers_with_same_host);

    peers_with_same_host_.resize(num_peers_with_same_host);
    for (auto&& i = 0U; i < num_peers_with_same_host; i++) {
        tracker_sock_->RecvInt(peers_with_same_host_[i]);
    }
    LOG_F(INFO, "Number of peers with same hostname : %d",
          num_peers_with_same_host);
    auto&& peers_with_same_host_str =
        str_utils::ConcatToString(peers_with_same_host_);
    LOG_F(INFO, "Peer ranks with the same host : {%s}",
          peers_with_same_host_str.c_str());
    std::sort(peers_with_same_host_.begin(), peers_with_same_host_.end());
    tracker_sock_->RecvInt(world_size_);
    LOG_F(INFO, "World size : %d", world_size_);
    // recieve my new rank from tracker
    tracker_sock_->RecvInt(rank_);
    LOG_F(INFO, "My New rank : %d", rank_);
    // get number of to connect and number of to accept nodes from tracker
    tracker_sock_->RecvInt(num_conn_);

    LOG_F(INFO, "Number peers need to connect at Node %d : %d", rank_,
          num_conn_);
    tracker_sock_->RecvInt(num_accept_);

    LOG_F(INFO, "Number peers need to be accepted at Node %d : %d", rank_,
          num_accept_);
    peer_addrs_.clear();

    peer_conn_.clear();
    peer_conn_.resize(num_conn_);
    for (int i = 0; i < num_conn_; ++i) {
        std::string haddr;
        tracker_sock_->RecvStr(haddr);
        tracker_sock_->RecvInt(peer_conn_[i]);
        auto&& hrank = peer_conn_[i];
        peer_addrs_[hrank] = haddr;
        LOG_F(INFO, "MyRank %d, PeerRank %d  with address %s", rank_, hrank,
              haddr.c_str());
    }
    peer_accept_.resize(num_accept_);
    for (int i = 0; i < num_accept_; i++) {
        tracker_sock_->RecvInt(peer_accept_[i]);
    }
    auto&& peer_accept_str = str_utils::ConcatToString(peer_accept_);
    LOG_F(INFO, "Peers to be accepted %s", peer_accept_str.c_str());
    tracker_lock_->unlock();
    tracker_sema_.Signal();
    return std::tie(num_conn_, num_accept_);
}

void Tracker::TrackerPrint(const std::string& msg) {
    tracker_lock_->lock();
    SendStr(std::string("print"));
    SendStr(msg);
    tracker_lock_->unlock();
}

}  // namespace comm
}  // namespace rdc
