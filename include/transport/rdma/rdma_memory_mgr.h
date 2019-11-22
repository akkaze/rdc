#pragma once
#include <infiniband/verbs.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

#include "transport/rdma/rdma_adapter.h"
namespace rdc {
inline void MRDeleter(ibv_mr* mr) {
    if (mr) {
        ibv_dereg_mr(mr);
    }
}
using MemoryRegionPtr = std::unique_ptr<ibv_mr, decltype(&MRDeleter)>;

static bool Comparator(const void* ptr, const MemoryRegionPtr& other) {
    return ptr < reinterpret_cast<char*>(other->addr) + other->length;
}
class RdmaMemoryMgr {
   public:
    static RdmaMemoryMgr* Get() {
        static RdmaMemoryMgr mgr;
        return &mgr;
    }
    ibv_mr* FindMemoryRegion(void* addr, size_t length) {
        std::lock_guard<std::mutex> lg(mrs_lock_);
        auto iter =
            std::upper_bound(mrs_.begin(), mrs_.end(), addr, &Comparator);
        if (iter == std::end(mrs_) || iter->get()->addr > addr) {
            return nullptr;
        } else {
            return iter->get();
        }
    }
    ibv_mr* InsertMemoryRegion(void* addr, size_t length,
                               const std::string& allocator_name = "") {
        ibv_mr* mr = nullptr;
        if (length == 0) return mr;
        mr = ibv_reg_mr(RdmaAdapter::Get()->protection_domain(), addr, length,
                        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
        LOG(INFO) << "Insert memory region 0x" << std::hex << mr->rkey << ". ["
                  << addr << "-" << (void*)((uint64_t)addr + length - 1) << "]"
                  << " SIZE: 0x" << length << " (" << allocator_name << ").";
        if (mr != nullptr) {
            std::lock_guard<std::mutex> lg(mrs_lock_);
            auto iter =
                std::upper_bound(mrs_.begin(), mrs_.end(), addr, &Comparator);
            mrs_.insert(iter, {mr, &MRDeleter});
        } else {
            LOG(WARNING) << "Cannot register memory region";
        }
        return mr;
    }
    ibv_mr* FindOrInsert(void* addr, size_t length) {
        auto mr = FindMemoryRegion(addr, length);
        if (mr) {
            return mr;
        } else {
            mr = InsertMemoryRegion(addr, length);
            return mr;
        }
    }
    void RemoveMemoryRegion(void* addr, size_t length) {
        if (length == 0) return;
        std::lock_guard<std::mutex> lg(mrs_lock_);
        auto iter =
            std::upper_bound(mrs_.begin(), mrs_.end(), addr, &Comparator);
        if (iter != std::end(mrs_) && iter->get()->addr == addr) {
            LOG(INFO) << "Evict memory region 0x" << std::hex
                      << iter->get()->rkey;
            mrs_.erase(iter);
        } else {
            LOG(WARNING) << "Failed to de-register memory region";
        }
    }

   private:
    std::mutex mrs_lock_;
    std::vector<MemoryRegionPtr> mrs_;
};
}  // namespace rdc
