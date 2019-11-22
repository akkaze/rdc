/*!
 * Copyright by Contributors
 * \file rdc-inl.h
 * \brief implementation of inline template function for rdc interface
 *
 * \author Ankun Zheng
 */
#pragma once
// use comm for implementation
#include <string>
#include <vector>
#include "comm/communicator_manager.h"
#include "comm/tracker.h"
#include "core/mpi.h"
#include "io/io.h"
#include "io/memory_io.h"
#include "rdc.h"
#include "utils/utils.h"
namespace rdc {
// intialize the rdc comm
inline void Init(int argc, char **argv) {
    comm::CommunicatorManager::Get()->Init(argc, argv);
}
// create a new rdc comm
inline comm::ICommunicator *NewCommunicator(const std::string &name) {
    return comm::CommunicatorManager::Get()->NewCommunicator(name).get();
}
// get an existed rdc comm
inline comm::ICommunicator *GetCommunicator(const std::string &name) {
    return comm::CommunicatorManager::Get()->GetCommunicator(name);
}
// finalize the rdc comm
inline void Finalize() {
    comm::CommunicatorManager::Get()->Finalize();
}
// get the rank of current process
inline int GetRank() {
    return comm::Tracker::Get()->rank();
}
// the the size of the world
inline int GetWorldSize() {
    return comm::Tracker::Get()->world_size();
}
// whether rdc is distributed
inline bool IsDistributed() {
    return comm::Tracker::Get()->IsDistributed();
}

// print message to the tracker
inline void TrackerPrint(const std::string &msg) {
    comm::Tracker::Get()->TrackerPrint(msg);
}
inline void Send(const Buffer &sendbuf, int dest) {
    comm::CommunicatorManager::Get()->GetCommunicator()->Send(sendbuf, dest);
}
inline void Recv(Buffer &recvbuf, int src) {
    comm::CommunicatorManager::Get()->GetCommunicator()->Recv(recvbuf, src);
}
inline void Send(void *send_data, uint64_t size, int dest) {
    comm::CommunicatorManager::Get()->GetCommunicator()->Send(send_data, size,
                                                              dest);
}
inline void Recv(void *recv_data, uint64_t size, int src) {
    comm::CommunicatorManager::Get()->GetCommunicator()->Recv(recv_data, size,
                                                              src);
}
inline void Barrier() {
    comm::CommunicatorManager::Get()->GetCommunicator()->Barrier();
}
// broadcast data to all other nodes from root
inline void Broadcast(Buffer &sendrecvbuf, int root,
                      const std::string &comm_name) {
    comm::CommunicatorManager::Get()->GetCommunicator(comm_name)->Broadcast(
        sendrecvbuf, root);
}
inline void Broadcast(void *sendrecvaddr, uint64_t size, int root,
                      const std::string &comm_name) {
    comm::CommunicatorManager::Get()->GetCommunicator(comm_name)->Broadcast(
        sendrecvaddr, size, root);
}
template <typename DType>
inline void Broadcast(std::vector<DType> &sendrecv_data, int root,
                      const std::string &comm_name) {
    uint64_t size = sendrecv_data.size();
    Broadcast(&size, sizeof(size), root, comm_name);
    if (sendrecv_data.size() != size) {
        sendrecv_data.resize(size);
    }
    if (size != 0) {
        Broadcast({utils::BeginPtr(sendrecv_data), size * sizeof(DType)}, root,
                  comm_name);
    }
}
inline void Broadcast(std::string &sendrecv_data, int root,
                      const std::string &comm_name) {
    size_t size = sendrecv_data.length();
    Broadcast(&size, sizeof(size), root);
    if (sendrecv_data.length() != size) {
        sendrecv_data.resize(size);
    }
    if (size != 0) {
        Broadcast(utils::BeginPtr(sendrecv_data), size * sizeof(char), root,
                  comm_name);
    }
}
inline void Allgather(std::vector<Buffer> &sendrecvbufs,
                      const std::string &comm_name) {
    comm::CommunicatorManager::Get()->GetCommunicator(comm_name)->Allgather(
        sendrecvbufs);
}
template <typename DType>
inline void Allgather(std::vector<std::vector<DType>> &sendrecv_data,
                      const std::string &comm_name) {
    std::vector<Buffer> sendrecvbufs(sendrecv_data.size());
    for (auto i = 0U; i < sendrecv_data.size(); ++i) {
        sendrecvbufs[i].set_addr(
            reinterpret_cast<void *>(utils::BeginPtr(sendrecv_data[i])));
        sendrecvbufs[i].set_size_in_bytes(sendrecv_data[i].size() *
                                          sizeof(DType));
    }
    Allgather(sendrecvbufs, comm_name);
}

// perform inplace Allreduce
template <typename OP, typename DType>
inline void Allreduce(DType *sendrecvbuf_, uint64_t count,
                      const std::string &comm_name) {
    Buffer sendrecvbuf(sendrecvbuf_, count * sizeof(DType));
    sendrecvbuf.set_item_size(sizeof(DType));
    auto reducer = [](Buffer src, Buffer dst) {
        op::Reducer<OP, DType>(src.addr(), dst.addr(), src.Count());
    };
    comm::Allreduce_(sendrecvbuf, reducer, mpi::GetType<DType>(), OP::kType,
                     comm_name);
}

// ---------------------------------
// Code to handle customized Reduce
// ---------------------------------
// function to perform reduction for Reducer
template <typename DType, void (*freduce)(DType &dst, const DType &src)>
inline void ReducerSafe_(const void *src_, void *dst_, int len_,
                         const MPI::Datatype &dtype) {
    const size_t kUnit = sizeof(DType);
    const char *psrc = reinterpret_cast<const char *>(src_);
    char *pdst = reinterpret_cast<char *>(dst_);
    DType tdst, tsrc;
    for (int i = 0; i < len_; ++i) {
        // use memcpy to avoid alignment issue
        std::memcpy(&tdst, pdst + i * kUnit, sizeof(tdst));
        std::memcpy(&tsrc, psrc + i * kUnit, sizeof(tsrc));
        freduce(tdst, tsrc);
        std::memcpy(pdst + i * kUnit, &tdst, sizeof(tdst));
    }
}
// function to perform reduction for Reducer
template <typename DType,
          void (*freduce)(DType &dst, const DType &src)>  // NOLINT(*)
inline void ReducerAlign_(const void *src_, void *dst_, int len_,
                          const MPI::Datatype &dtype) {
    const DType *psrc = reinterpret_cast<const DType *>(src_);
    DType *pdst = reinterpret_cast<DType *>(dst_);
    for (int i = 0; i < len_; ++i) {
        freduce(pdst[i], psrc[i]);
    }
}
// closure to call Allreduce
template <typename DType>
struct SerializeReduceClosure {
    DType *sendrecvobj;
    size_t max_nbyte, count;
    std::string *p_buffer;
    // invoke the closure
    inline void Run(void) {
        for (size_t i = 0; i < count; ++i) {
            MemoryFixedSizeStream fs(utils::BeginPtr(*p_buffer) + i * max_nbyte,
                                     max_nbyte);
            sendrecvobj[i].Save(fs);
        }
    }
    inline static void Invoke(void *c) {
        static_cast<SerializeReduceClosure<DType> *>(c)->Run();
    }
};
template <typename DType,
          void (*freduce)(DType &dst, const DType &src)>  // NOLINT(*)
inline void Reducer<DType, freduce>::Allreduce(DType *sendrecvbuf,
                                               size_t count) {
    this->Allreduce(sendrecvbuf, count);
}
template <typename DType>
inline void SerializeReducer<DType>::Allreduce(DType *sendrecvobj,
                                               size_t max_nbytes,
                                               size_t count) {
    this->Allreduce(sendrecvobj, max_nbytes, count);
}

inline void AddGlobalState(const std::string &name, void *ptr, size_t size) {
    comm::CommunicatorManager::Get()->AddGlobalState(name, ptr, size);
}

inline void AddLocalState(const std::string &name, void *ptr, size_t size) {
    comm::CommunicatorManager::Get()->AddLocalState(name, ptr, size);
}

inline int LoadCheckPoint() {
    return comm::CommunicatorManager::Get()->LoadCheckPoint();
}

inline void CheckPoint() {
    comm::CommunicatorManager::Get()->CheckPoint();
}

inline bool DetectDeadNodes() {
    return comm::Tracker::Get()->num_dead_nodes() > 0;
}

inline bool DetectPendingNodes() {
    return comm::Tracker::Get()->num_pending_nodes() > 0;
}

inline bool Reset() {
    comm::Tracker::Get()->Connect("start");
    comm::CommunicatorManager::Get()->ResetAllCommunicators();
}
}  // namespace rdc
