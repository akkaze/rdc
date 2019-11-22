#include <chrono>
#include <thread>
#include "comm/communicator_manager.h"
#include "comm/deamon.h"
#include "comm/tracker.h"
#include "common/env.h"
#include "core/logging.h"
namespace rdc {
namespace comm {
Deamon::Deamon() {
    heartbeat_interval_ = CommunicatorManager::Get()->heartbeat_interval();
    if (heartbeat_interval_ <= 0) {
        heartbeat_interval_ = 60000;
    }
    // spin util connected to tracker
    heartbeat_thrd_.reset(new std::thread(&Deamon::Heartbeat, this));
}

void Deamon::Shutdown() {
    heartbeat_thrd_->join();
}

void Deamon::Heartbeat() {
    while (!Tracker::Get()->tracker_connected()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(heartbeat_interval_));
    }
    while (Tracker::Get()->tracker_connected()) {
        Tracker::Get()->Lock();
        if (!Tracker::Get()->tracker_connected()) {
            Tracker::Get()->UnLock();
            break;
        }
        Tracker::Get()->SendStr("heartbeat");
        std::string heartbeat_token;
        Tracker::Get()->RecvStr(heartbeat_token);
        CHECK_EQ(heartbeat_token, "heartbeat_done");
        Tracker::Get()->RecvInt(num_dead_nodes_);
        if (num_dead_nodes_ > 0) {
            Tracker::Get()->set_num_dead_nodes(num_dead_nodes_);
            dead_nodes_.clear();
            dead_nodes_.resize(num_dead_nodes_);
            for (auto&& d = 0; d < num_dead_nodes_; d++) {
                Tracker::Get()->RecvInt(dead_nodes_[d]);
            }
            Tracker::Get()->set_dead_nodes(dead_nodes_);
        }
        Tracker::Get()->RecvInt(num_pending_nodes_);
        Tracker::Get()->set_num_pending_nodes(num_pending_nodes_);
        if (CommunicatorManager::Get()->restart()) {
            num_pending_nodes_ = 0;
            num_dead_nodes_ = 0;
            dead_nodes_.clear();
        }
        if (num_pending_nodes_ > 0) {
            LOG_F(INFO, "detect %d dead nodes", num_dead_nodes_);
        }
        if (num_pending_nodes_ > 0) {
            LOG_F(INFO, "detect %d pending nodes", num_pending_nodes_);
        }
        Tracker::Get()->set_num_pending_nodes(num_pending_nodes_);

        last_heartbeat_timepoint_ =
            std::chrono::steady_clock::now().time_since_epoch().count();
        Tracker::Get()->UnLock();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(heartbeat_interval_));
    }
}
}  // namespace comm
}  // namespace rdc
