#pragma once
#include <memory>
#include <vector>
namespace rdc {
namespace comm {
class Deamon {
public:
    Deamon();

    Deamon(const Deamon& deamon) = default;

    Deamon(Deamon&& deamon) = default;

    ~Deamon() = default;

    /**
     * @brief:keep hearbeat with tracker
     */
    void Heartbeat();

    void Shutdown();

    int num_dead_nodes() const {
        return num_dead_nodes_;
    }

    std::vector<int> dead_nodes() const {
        return dead_nodes_;
    }

    int num_pending_nodes() const {
        return num_pending_nodes_;
    }
private:
    // heartbeat related members
    /*!brief interval between two heartbeats*/
    uint64_t heartbeat_interval_;
    /*!brief background thread for keeping heartbeats*/
    std::unique_ptr<std::thread> heartbeat_thrd_;
    /*!brief timepoint of last valid heartbeat*/
    uint64_t last_heartbeat_timepoint_;
    int num_dead_nodes_ = -1;
    std::vector<int> dead_nodes_;
    int num_pending_nodes_ = -1;
};
}  // namespace comm
}  // namespace rdc
