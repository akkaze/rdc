#pragma once
#ifdef RDC_USE_RDMA
#include <infiniband/verbs.h>
#include "transport/rdma/rdma_memory_mgr.h"
#endif
#ifdef RDC_USE_SHMEM
#include "transport/ipc/shm.h"
#endif
#include "common/env.h"
#include "common/object_pool.h"
#include "common/pool.h"
#include "core/logging.h"

namespace rdc {
class Buffer : public ObjectPoolAllocatable<Buffer> {
public:
    Buffer(){};
    Buffer(uint64_t size_in_bytes) : size_in_bytes_(size_in_bytes) {
    }
    Buffer(void* addr, uint64_t size_in_bytes)
        : Buffer(addr, size_in_bytes, 0, size_in_bytes) {
    }
    Buffer(const void* addr, uint64_t size_in_bytes)
        : Buffer(addr, size_in_bytes, 0, size_in_bytes) {
    }
    Buffer(void* addr, uint64_t size_in_bytes, const bool& pinned)
        : Buffer(addr, size_in_bytes, pinned, 0, size_in_bytes) {
    }
    Buffer(const void* addr, uint64_t size_in_bytes, const bool& pinned)
        : Buffer(addr, size_in_bytes, pinned, 0, size_in_bytes) {
    }
    Buffer(void* addr, uint64_t size_in_bytes, uint64_t start, uint64_t end)
        : Buffer(addr, size_in_bytes, start, end, false) {
    }
    Buffer(const void* addr, uint64_t size_in_bytes, uint64_t start,
           uint64_t end)
        : Buffer(addr, size_in_bytes, start, end, false) {
    }
    Buffer(void* addr, uint64_t size_in_bytes, uint64_t start, uint64_t end,
           const bool& pinned)
        : addr_(addr),
          size_in_bytes_(size_in_bytes),
          is_mutable_(true),
          start_(start),
          end_(end),
          pinned_(pinned) {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#endif
        bool use_rdma = Env::Get()->GetEnv("RDC_USE_RDMA", 0);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif
#ifdef RDC_USE_RDMA
        if (pinned && use_rdma) {
            memory_region_ =
                RdmaMemoryMgr::Get()->InsertMemoryRegion(addr_, size_in_bytes);
        }
#endif
    }
    Buffer(const void* addr, uint64_t size_in_bytes, uint64_t start,
           uint64_t end, const bool& pinned)
        : addr_(const_cast<void*>(addr)),
          size_in_bytes_(size_in_bytes),
          is_mutable_(false),
          start_(start),
          end_(end),
          pinned_(pinned) {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#endif

        bool use_rdma = Env::Get()->GetEnv("RDC_USE_RDMA", 0);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

#ifdef RDC_USE_RDMA
        if (pinned && use_rdma) {
            memory_region_ =
                RdmaMemoryMgr::Get()->InsertMemoryRegion(addr_, size_in_bytes);
        }
#endif
    }

#if RDC_USE_SHMEM
    ~Buffer() {
        if (use_shm_) {
            shm_->Close();
        }
    }
    void set_memfile_name(const std::string memfile_name) {
        memfile_name_ = memfile_name;
    }
    void set_shm(std::shared_ptr<Shmem> shm) {
        shm_ = shm;
        use_shm_ = true;
    }
#endif
    Buffer Slice(const uint64_t& start, const uint64_t& end) const {
        Buffer subbuffer((void*)((int8_t*)addr_ + start), end - start, start,
                         end);
        subbuffer.set_with_type(with_type_);
        subbuffer.set_item_size(item_size_);
        subbuffer.set_is_mutable(is_mutable_);
        return subbuffer;
    }
    template <typename DType>
    DType* As() const {
        return reinterpret_cast<DType*>(addr_);
    }
    template <typename DType>
    DType* At(const uint32_t& index) {
        return reinterpret_cast<DType*>(addr_) + index;
    }
    template <typename DType>
    std::string DebugString() const {
        CHECK(std::is_pod<DType>::value);
        const auto& item_size = sizeof(DType);
        const auto& count = size_in_bytes_ / item_size;
        DType* typed_addr = this->template As<DType>();
        std::string debug_string;
        for (auto i = 0U; i < count; i++) {
            debug_string += std::to_string(*(typed_addr + i));
            if (i != count - 1) {
                debug_string += '\t';
            }
        }
        return debug_string;
    }
    void* addr() const {
        return addr_;
    }
    void set_addr(void* addr) {
        addr_ = addr;
        is_mutable_ = true;
    }
    void set_addr(const void* addr) {
        addr_ = const_cast<void*>(addr);
        is_mutable_ = false;
    }
    uint64_t size_in_bytes() const {
        return size_in_bytes_;
    }
    void set_size_in_bytes(const uint64_t& size_in_bytes) {
        size_in_bytes_ = size_in_bytes;
    }
    bool is_mutable() const {
        return is_mutable_;
    }
    void set_is_mutable(const bool& is_mutable) {
        is_mutable_ = is_mutable;
    }
#ifdef RDC_USE_RDMA
    ibv_mr* memory_region() const {
        return memory_region_;
    }
#endif
    uint64_t Count() const {
        CHECK(with_type_ && (size_in_bytes_ % item_size_ == 0));
        return size_in_bytes_ / item_size_;
    }
    bool with_type() const {
        return with_type_;
    }
    void set_with_type(const bool& with_type) {
        with_type_ = with_type;
    }
    uint64_t item_size() const {
        return item_size_;
    }
    void set_item_size(const uint64_t& item_size) {
        with_type_ = true;
        item_size_ = item_size;
    }
    void set_start(const uint64_t start) {
        start_ = start;
    }
    void set_end(const uint64_t end) {
        end_ = end;
    }
#if RDC_WITH_PYTHON
    std::string format() const {
        return format_;
    }
    void set_format(const std::string format) {
        format_ = format;
    }
    uint64_t stride() const {
        return stride_;
    }
    void set_stride(const uint64_t& stride) {
        stride_ = stride;
    }
#endif
    void AllocTemp(const std::function<void*(const uint64_t&)>& alloc_func) {
        own_data_ = true;
        temp_ = true;
        addr_ = alloc_func(size_in_bytes_);
    }

    void FreeTemp(const std::function<void(void*)>& free_func) {
        own_data_ = false;
        temp_ = false;
        free_func(addr_);
        addr_ = nullptr;
    }
    void Alloc() {
        own_data_ = true;
        temp_ = true;
        addr_ = GPool().allocate(size_in_bytes_);
    }
    void Free() {
        own_data_ = false;
        temp_ = false;
        GPool().deallocate(addr_, size_in_bytes_);
        addr_ = nullptr;
    }

private:
    void* addr_;
    uint64_t size_in_bytes_;
    bool is_mutable_;
    bool temp_;
    bool with_type_;
    uint64_t item_size_;
    std::string data_type_;
    bool own_data_;
    uint64_t start_;
    uint64_t end_;
    bool pinned_;
#if RDC_WITH_PYTHON
    std::string format_;
    uint64_t stride_;
#endif
#ifdef RDC_USE_RDMA
    ibv_mr* memory_region_;
#endif

#ifdef RDC_USE_SHMEM
    std::string memfile_name_;
    std::shared_ptr<Shmem> shm_;
    bool use_shm_;
#endif
};
}  // namespace rdc
