#include "core/work_request.h"

namespace rdc {
WorkRequest::WorkRequest()
    : status_(WorkStatus::kPending), processed_bytes_upto_now_(0) {
}

WorkRequest::WorkRequest(const uint64_t& req_id, const WorkType& work_type,
                         void* ptr, const size_t& size)
    : req_id_(req_id),
      work_type_(work_type),
      status_(WorkStatus::kPending),
      ptr_(ptr),
      size_in_bytes_(size),
      processed_bytes_upto_now_(0) {
}

WorkRequest::WorkRequest(const uint64_t& req_id, const WorkType& work_type,
                         const void* ptr, const size_t& size)
    : req_id_(req_id),
      work_type_(work_type),
      status_(WorkStatus::kPending),
      ptr_(const_cast<void*>(ptr)),
      size_in_bytes_(size),
      processed_bytes_upto_now_(0) {
}

WorkRequest::WorkRequest(const WorkRequest& other) {
    req_id_ = other.req_id_;
    ptr_ = other.ptr_;
    size_in_bytes_ = other.size_in_bytes_;
    work_type_ = other.work_type_;
    processed_bytes_upto_now_ = other.processed_bytes_upto_now_;
    set_status(other.status());
    extra_data_ = other.extra_data_;
}

WorkRequest& WorkRequest::operator=(const WorkRequest& other) {
    req_id_ = other.req_id_;
    ptr_ = other.ptr_;
    size_in_bytes_ = other.size_in_bytes_;
    work_type_ = other.work_type_;
    processed_bytes_upto_now_ = other.processed_bytes_upto_now_;
    set_status(other.status());
    extra_data_ = other.extra_data_;
    return *this;
}

WorkStatus WorkRequest::status() const {
    return status_.load(std::memory_order_acquire);
}

void WorkRequest::set_status(const WorkStatus& status) {
    status_.store(status, std::memory_order_release);
    return;
}

bool WorkRequest::AddBytes(const size_t nbytes) {
    processed_bytes_upto_now_ += nbytes;
    if (processed_bytes_upto_now_ == size_in_bytes_) {
        status_ = WorkStatus::kFinished;
        return true;
    }
    return false;
}

void WorkRequest::Wait() {
    if ((status_.load(std::memory_order_acquire) != WorkStatus::kFinished) &&
           (status_.load(std::memory_order_acquire) != WorkStatus::kError)) {
        sema_.Wait();
    }
}

void WorkRequest::Notify() {
    sema_.Signal();
}

size_t WorkRequest::size_in_bytes() const {
    return size_in_bytes_;
}

size_t WorkRequest::processed_bytes_upto_now() const {
    return processed_bytes_upto_now_;
}

size_t WorkRequest::remain_nbytes() const {
    return size_in_bytes_ - processed_bytes_upto_now_;
}

uint64_t WorkRequest::id() const {
    return req_id_;
}

WorkType WorkRequest::work_type() const {
    return work_type_;
}

void* WorkRequest::ptr() {
    return ptr_;
}

WorkRequestManager::WorkRequestManager() {
    store_lock_ = utils::make_unique<utils::SpinLock>();
    id_lock_ = utils::make_unique<utils::SpinLock>();
    cur_req_id_ = 0;
}

WorkRequestManager* WorkRequestManager::Get() {
    static WorkRequestManager mgr;
    return &mgr;
}

void WorkRequestManager::AddWorkRequest(const WorkRequest& req) {
    store_lock_->lock();
    // note: a work request copy will be triggered here
    all_work_reqs[req.id()] = req;
    store_lock_->unlock();
}

uint64_t WorkRequestManager::NewWorkRequest(const WorkType& work_type,
                                            void* ptr, const size_t& size) {
    id_lock_->lock();
    cur_req_id_++;
    WorkRequest work_req(cur_req_id_, work_type, ptr, size);
    id_lock_->unlock();
    AddWorkRequest(work_req);
    return work_req.id();
}

uint64_t WorkRequestManager::NewWorkRequest(const WorkType& work_type,
                                            const void* ptr,
                                            const size_t& size) {
    id_lock_->lock();
    cur_req_id_++;
    WorkRequest work_req(cur_req_id_, work_type, ptr, size);
    id_lock_->unlock();
    AddWorkRequest(work_req);
    return work_req.id();
}

WorkRequest& WorkRequestManager::GetWorkRequest(const uint64_t& req_id) {
    std::lock_guard<utils::SpinLock> lg(*store_lock_);
    return all_work_reqs[req_id];
}

bool WorkRequestManager::AddBytes(const uint64_t& req_id, size_t nbytes) {
    return all_work_reqs[req_id].AddBytes(nbytes);
}

bool WorkRequestManager::Contain(const uint64_t& req_id) {
    return all_work_reqs.count(req_id);
}

void WorkRequestManager::Wait(const uint64_t& req_id) {
    store_lock_->lock();
    auto& work_req = all_work_reqs[req_id];
    store_lock_->unlock();
    work_req.Wait();
}

size_t WorkRequestManager::processed_bytes_upto_now(const uint64_t& req_id) {
    return all_work_reqs[req_id].processed_bytes_upto_now();
}

WorkStatus WorkRequestManager::status(const uint64_t& req_id) {
    return all_work_reqs[req_id].status();
}

void WorkRequestManager::set_status(const uint64_t& req_id,
                                    const WorkStatus& status) {
    all_work_reqs[req_id].set_status(status);
}

WorkCompletion::WorkCompletion(const uint64_t& id)
    : id_(id), processed_bytes_upto_now_(0) {
}

void WorkCompletion::Wait() {
    CHECK(WorkRequestManager::Get()->Contain(id_));
    WorkRequestManager::Get()->Wait(id_);
}

WorkStatus WorkCompletion::status() {
    // only query once
    if (WorkRequestManager::Get()->Contain(id_)) {
        status_ = WorkRequestManager::Get()->status(id_);
    }
    return status_;
}

ChainWorkCompletion::~ChainWorkCompletion() {
    for (auto& work_comp : this->work_comps_) {
        WorkCompletion::Delete(work_comp);
    }
}

void ChainWorkCompletion::Push(WorkCompletion* work_comp) {
    work_comps_.emplace_back(work_comp);
}

void ChainWorkCompletion::Wait() {
    for (auto& work_comp : work_comps_) {
        work_comp->Wait();
    }
}

WorkStatus ChainWorkCompletion::status() {
    for (auto& work_comp : work_comps_) {
        if (work_comp->status() != WorkStatus::kFinished) {
            return work_comp->status();
        }
    }
    return WorkStatus::kFinished;
}
}  // namespace rdc
