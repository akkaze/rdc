#pragma once
#include "comm/checkpointer.h"
#include "comm/communicator.h"
#include "comm/deamon.h"
#include "comm/tracker.h"
#include "utils/lock_utils.h"

namespace rdc {
namespace comm {
class CommunicatorManager {
public:
    CommunicatorManager();

    ~CommunicatorManager() = default;

    static CommunicatorManager* Get();

    static void Release();
    /*! @brief initializes the comm module */
    void Init(int argc, char** argv);

    /*! @brief finalizes the comm module */
    void Finalize();
    /*! @*brief rest all communicators*/
    void ResetAllCommunicators();
    /*!
     * @brief explicitly re-initialize everything before calling LoadCheckPoint
     *    call this function when ICommunicator throws an exception,
     *    this function should only be used for test purposes
     */
    void InitAfterException();
    /*!
     * @brief set parameters to the comm
     * @param name parameter name
     * @param val parameter value
     */
    void SetParam(const char* name, const char* val);

    void AddCommunicator(const std::string& name,
                         const std::shared_ptr<ICommunicator>& communicator);

    /**
     * @brief: create a new communicator with a specific name
     *
     * @param name new communicator name
     *
     * @return created new commnicator
     */
    std::shared_ptr<ICommunicator> NewCommunicator(const std::string& name);

    /**
     * @brief: gets a created communicator by name
     *
     * @param name communicator name
     *
     * @return communicator with given name
     */
    ICommunicator* GetCommunicator(const std::string& name = kMainCommName);

    void CreateDeamon();

    void AddGlobalState(const std::string& name, void* ptr, size_t size);
    void AddLocalState(const std::string& name, void* ptr, size_t size);

    void CheckPoint();
    int LoadCheckPoint();

    std::unordered_map<std::string, std::shared_ptr<ICommunicator>>
    communicators() const;

    std::vector<std::string> env_vars() const {
        return env_vars_;
    }

    std::string tracker_uri() const {
        return tracker_uri_;
    }

    int tracker_port() const {
        return tracker_port_;
    }

    size_t reduce_ring_mincount() const {
        return reduce_ring_mincount_;
    }
    int heartbeat_interval() const {
        return heartbeat_interval_;
    }
    bool restart() const {
        return restart_.load(std::memory_order_acquire);
    }
    void set_restart(const bool& restart) {
        restart_.store(restart, std::memory_order_release);
    }
private:
    // all communicators managed
    std::unordered_map<std::string, std::shared_ptr<ICommunicator>>
        communicators_;
    std::vector<std::string> comm_names_;
    // list of enviroment variables that are of possible interest
    std::vector<std::string> env_vars_;
    // uri of tracker
    std::string tracker_uri_;
    // port of tracker address
    int tracker_port_;

    std::unique_ptr<CheckPointer> checkpointer_;
    int world_size_;
    int connect_retry_;
    utils::SpinLock comm_lock_;
    // mininum count of cells to use ring based method
    size_t reduce_ring_mincount_;
    utils::SpinLock tracker_lock_;
    std::shared_ptr<Deamon> deamon_;
    std::thread demaon_thrd_;
    LightweightSemaphore tracker_sema_;

    int heartbeat_interval_;
    std::atomic<bool> restart_{false};
    static std::mutex create_mutex;
    static std::atomic<CommunicatorManager*> instance;
    static std::atomic<bool> created;
};

}  // namespace comm
}  // namespace rdc
