#pragma once
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "common/semaphore.h"
#include "transport/tcp/socket.h"
namespace rdc {
namespace comm {
class Tracker {
public:
    Tracker();

    Tracker(const std::string& tracker_uri, const int& tracker_port);

    ~Tracker();

    static Tracker* Get();

    static void Release();

    void Lock() const;

    void UnLock() const;

    void SendInt(const int32_t& value);

    void SendStr(const std::string& str);

    void SendBytes(void* buf, int32_t size);

    void RecvInt(int32_t& value);

    void RecvStr(std::string& str);

    void RecvBytes(void* buf, int32_t& size);

    bool IsDistributed() const {
        return true;
    }
    /*!
     * @brief print the msg in the tracker,
     *  this function can be used to communicate the information of the
     *  progress to the user who monitors the tracker
     * @param msg message to be
     */
    void TrackerPrint(const std::string& msg);
    void Send(void* buf, size_t size) {
        tracker_sock_->Send(buf, size);
    }
    void Recv(void* buf, size_t size) {
        tracker_sock_->Recv(buf, size);
    }
    bool IsClosed() const {
        return tracker_sock_->IsClosed();
    }
    //---------------------properties-------------------------
    std::string host_uri(void) const {
        return host_uri_;
    }

    bool tracker_connected() const {
        return tracker_connected_.load(std::memory_order_acquire);
    }

    void set_tracker_connected(const bool& tracker_connected) {
        tracker_connected_.store(tracker_connected, std::memory_order_release);
    }

    int worker_port() const {
        return worker_port_;
    }

    void set_worker_port(const int& worker_port) {
        worker_port_ = worker_port;
    }

    int world_size() const {
        return world_size_;
    }

    int rank() const {
        return rank_;
    }

    int num_conn() const {
        return num_conn_;
    }

    int num_accept() const {
        return num_accept_;
    }

    std::unordered_map<int, std::string> peer_addrs() const {
        return peer_addrs_;
    }

    std::string peer_addr(int i) {
        return peer_addrs_[peer_conn_[i]];
    }

    int num_dead_nodes() const {
        return num_dead_nodes_;
    }

    int num_pending_nodes() const {
        return num_pending_nodes_;
    }

    void set_dead_nodes(const std::vector<int>& dead_nodes) {
        dead_nodes_ = dead_nodes;
    }

    void set_num_dead_nodes(const int& num_dead_nodes) {
        num_dead_nodes_ = num_dead_nodes;
    }

    void set_num_pending_nodes(const int& num_pending_nodes) {
        num_pending_nodes_ = num_pending_nodes;
    }

    std::vector<int> peers_with_same_host() const {
        return peers_with_same_host_;
    }

    std::vector<int> peer_conn() const {
        return peer_conn_;
    }

    int peer_conn(int i) const {
        return peer_conn_[i];
    }

    std::vector<int> peer_accept() const {
        return peer_accept_;
    }

    int peer_accept(int i) const {
        return peer_accept_[i];
    }
    /*!
     * @brief initialize connection to the tracker
     * @return a channel that initializes the connection
     */
    std::tuple<int, int> Connect(const char* cmd = "start");

protected:
private:
    // uri of tracker
    std::string tracker_uri_;
    // port of tracker address
    int tracker_port_;
    // uri of current host, to be set by Init
    std::string host_uri_;
    // port of worker process
    int worker_port_, nport_trial_;
    int shmem_worker_port_;
    int world_size_;
    int rank_;
    int num_conn_, num_accept_;
    std::vector<int> peer_conn_, peer_accept_;
    // connect retry time
    int connect_retry_;
    // channel for communication with tracker_
    std::shared_ptr<TcpSocket> tracker_sock_;
    //! @breif addr of all peers
    std::unordered_map<int, std::string> peer_addrs_;

    std::vector<int> peers_with_same_host_;

    std::atomic<bool> tracker_connected_{false};

    std::atomic<bool> tracker_closed_;
    std::shared_ptr<std::mutex> tracker_lock_;
    LightweightSemaphore tracker_sema_;

    int num_pending_nodes_;
    int num_dead_nodes_;
    std::vector<int> dead_nodes_;
    static std::mutex create_mutex;
    static std::atomic<Tracker*> instance;
    static std::atomic<bool> created;
};
}  // namespace comm
}  // namespace rdc
