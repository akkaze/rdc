#pragma once
#include <unistd.h>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include "common/any.h"
#include "common/object_pool.h"
#include "common/semaphore.h"
#include "core/logging.h"
#include "utils/lock_utils.h"
#include "utils/utils.h"
namespace rdc {

enum class WorkType : uint32_t {
    kSend,
    kRecv,
};

enum class WorkStatus : uint32_t {
    kPending = 1 << 1,
    kRunning = 1 << 2,
    kFinished = 1 << 3,
    kCanceled = 1 << 4,
    kClosed = 1 << 5,
    kError = 1 << 6,
};

class WorkRequest {
public:
    WorkRequest();

    WorkRequest(const uint64_t& req_id, const WorkType& work_type, void* ptr,
                const size_t& size);

    WorkRequest(const uint64_t& req_id, const WorkType& work_type,
                const void* ptr, const size_t& size);

    ~WorkRequest() = default;

    WorkRequest(const WorkRequest& other);

    WorkRequest& operator=(const WorkRequest& other);

    /**
     * @brief: add number of bytes readed/wrote by last run to total completed
     * bytes
     *
     * @param nbytes number of bytes readed/wrote by channel
     * note: this function will be called by channel so it will be consumed by
     * single consumer
     * @return
     */
    bool AddBytes(const size_t nbytes);

    /**
     * @brief: return the underlying pointer at certain position with expected
     * datatype
     *
     * @tparam T expected underlying datatype pointed by this pointer
     * @param pos
     *
     * @return underlying pointer
     */
    template <typename T>
    T* pointer_at(const size_t& pos) {
        return reinterpret_cast<T*>(ptr_) + pos;
    }

    /**
     * @brief: wait this work request to finish, when wait return, this work
     * request is either finished, canceled, or the related channel is closed
     */
    void Wait();

    /**
     * @brief: notify wait to return
     */
    void Notify();
    /***********************properties********************************/
    size_t size_in_bytes() const;

    size_t processed_bytes_upto_now() const;

    size_t remain_nbytes() const;

    void set_status(const WorkStatus& status);

    WorkStatus status() const;

    uint64_t id() const;

    WorkType work_type() const;

    void* ptr();

    /**
     * @brief: set extra data
     *
     * @tparam T extra data datatype
     * @param extra_data
     */
    template <typename T>
    void set_extra_data(const T& extra_data) {
        extra_data_ = extra_data;
    }

    /**
     * @brief: get extra data
     *
     * @tparam T extra data data type
     */
    template <typename T>
    void extra_data() const {
        return any_cast<T>(extra_data_);
    }

private:
    /*! @brief request id which start from 0 */
    uint64_t req_id_;
    /*! @brief work request type, either send or recv*/
    WorkType work_type_;
    /*! @underlying type erased pointer*/
    void* ptr_;
    /*! @brief total size to be read/write of current work request*/
    size_t size_in_bytes_;
    /*! @brief current readed/written bytes */
    size_t processed_bytes_upto_now_;
    /*! @brief current status of this work request */
    std::atomic<WorkStatus> status_;
    /*! @brief extra data passed to this work requst when created*/
    any extra_data_;
    /*! @brief needed when wait*/
    LightweightSemaphore sema_;
    std::function<void()> done_callback_;
};

class WorkRequestManager {
public:
    WorkRequestManager();

    ~WorkRequestManager() = default;

    explicit WorkRequestManager(const WorkRequestManager&) = default;

    WorkRequestManager(WorkRequestManager&&) = default;

    static WorkRequestManager* Get();

    /**
     * @brief: add the work requst to manager
     * note: a copy will be invoked here, be careful of it
     * @param req the work requst need to be added to manager
     */
    void AddWorkRequest(const WorkRequest& req);

    /**
     * @brief: create a new work requst and the assign a work requst id to id
     *
     * @param work_type work requst type, either send/recv
     * @param ptr underlying pointer
     * @param size size of buffer need to be processed
     *
     * @return work requst id, since it is the unique token for acquring the
     * work requst in manager
     */
    uint64_t NewWorkRequest(const WorkType& work_type, void* ptr,
                            const size_t& size);

    uint64_t NewWorkRequest(const WorkType& work_type, const void* ptr,
                            const size_t& size);

    /**
     * @brief: create a new work reqeust and assign a work request id to it
     *
     * @tparam T extra data type
     * @param work_type work request type, either send/recv
     * @param ptr underlying pointer
     * @param size size of buffer need to be processed
     * @param extra_data extra data
     *
     * @return
     */
    template <typename T>
    uint64_t NewWorkRequest(const WorkType& work_type, void* ptr,
                            const size_t& size, const T& extra_data) {
        id_lock_->lock();
        cur_req_id_++;
        WorkRequest work_req(cur_req_id_, work_type, ptr, size);
        work_req.set_extra_data(extra_data);
        id_lock_->unlock();
        AddWorkRequest(work_req);
        return work_req.id();
    }

    template <typename T>
    uint64_t NewWorkRequest(const WorkType& work_type, const void* ptr,
                            const size_t& size, const T& extra_data) {
        id_lock_->lock();
        cur_req_id_++;
        WorkRequest work_req(cur_req_id_, work_type, ptr, size);
        work_req.set_extra_data(extra_data);
        id_lock_->unlock();
        AddWorkRequest(work_req);
        return work_req.id();
    }

    WorkRequest& GetWorkRequest(const uint64_t& req_id);

    bool Contain(const uint64_t& req_id);

    /*note: the following method will be triggered through the underlying
     * work request, so the unique id is required*/
    void Wait(const uint64_t& req_id);

    bool AddBytes(const uint64_t& req_id, size_t nbytes);

    size_t processed_bytes_upto_now(const uint64_t& req_id);

    WorkStatus status(const uint64_t& req_id);

    void set_status(const uint64_t& req_id, const WorkStatus& status);

private:
    uint64_t cur_req_id_;

    std::unordered_map<uint64_t, WorkRequest> all_work_reqs;

    /*! @brief lock needed when add a new work requst to work requests hashmap*/
    std::unique_ptr<utils::SpinLock> store_lock_;
    /*! @brief lock needed when generate a globally consistent auto increment
     * since it only use few instructions, a spinlock is enough for this
     * operation*/
    std::unique_ptr<utils::SpinLock> id_lock_;
    // used when we do not want workrequest shutdown by themselves, deprecated
};

/**
 * @brief: a work completion is associated with a work request, indicate
 * the status of that work request
 */
class WorkCompletion : public ObjectPoolAllocatable<WorkCompletion> {
public:
    WorkCompletion(const uint64_t& id);

    WorkCompletion(const WorkCompletion& other) = default;

    WorkCompletion(WorkCompletion&& other) = default;

    /**
     * @brief: get the id of the corresponding work request
     *
     * @return id of the corresponding work request
     */
    uint64_t WorkRequstId() const;

    /**
     * @brief: invoke waiting of the underlying work request
     */
    void Wait();
    /**
     * @brief: get the status of the corresponding work request
     *
     * @return status of corresponding work request
     */
    WorkStatus status();

private:
    uint64_t id_;
    size_t processed_bytes_upto_now_;
    WorkStatus status_;
    bool is_status_setted_;
};

class ChainWorkCompletion : public ObjectPoolAllocatable<ChainWorkCompletion> {
public:
    ChainWorkCompletion() = default;

    ~ChainWorkCompletion();

    void Push(WorkCompletion* work_comp);

    bool done();

    void Wait();

    WorkStatus status();

private:
    std::vector<WorkCompletion*> work_comps_;
};
}  // namespace rdc
