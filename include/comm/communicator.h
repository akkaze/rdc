/*!
 *  Copyright (c) 2018 by Contributors
 * \file comm.h
 * @brief This file defines the core interface of rdc library
 * \author Ankun Zheng
 */
#pragma once
#include <memory>
#include <string>
#include "core/mpi.h"
#include "core/work_request.h"
#include "io/io.h"
#include "transport/buffer.h"
namespace MPI {
/*! @brief MPI data type just to be compatible with MPI reduce function*/
class Datatype;
}  // namespace MPI

/*! @brief namespace of rdc */
namespace rdc {
const std::string kMainCommName = "main";
/*! @brief core interface of the comm */
namespace comm {
/*!
 * @brief reduce function, the same form of MPI reduce function is used,
 *        to be compatible with MPI interface
 *        In all the functions, the memory is ensured to aligned to 64-bit
 *        which means it is OK to cast src,dst to double* int* etc
 * @param src pointer to source space
 * @param dst pointer to destination reduction
 * @param count total number of elements to be reduced (note this is total
 * number of elements instead of bytes) the definition of the reduce function
 * should be type aware
 * @param dtype the data type object, to be compatible with MPI reduce
 */
using ReduceFunction = std::function<void(Buffer src, Buffer dst)>;
using RawReduceFunction =
    std::function<void(const void* src, void* dst, uint64_t len)>;

/*! @brief interface of core Allreduce comm */
class ICommunicator {
public:
    ICommunicator() = default;
    /*! @brief  destructor */
    virtual ~ICommunicator() = default;

    void Init(int world_size, int num_conn, int num_accept);

    virtual void ReConnectLinks(
        const std::tuple<int, int>& num_conn_accept) = 0;

    virtual void ResetLinks() = 0;

    virtual void Shutdown() = 0;

    virtual void Send(Buffer sendbuf, int dest) = 0;

    virtual void Recv(Buffer recvbuf, int src) = 0;

    void Send(void* sendaddr, uint64_t size_in_bytes, int dest) {
        Buffer sendbuf(sendaddr, size_in_bytes);
        return this->Send(sendbuf, dest);
    }
    void Recv(void* recvaddr, uint64_t size_in_bytes, int src) {
        Buffer recvbuf(recvaddr, size_in_bytes);
        return this->Recv(recvbuf, src);
    }

    virtual WorkCompletion* ISend(Buffer sendbuf, int dest) = 0;

    virtual WorkCompletion* IRecv(Buffer recvbuf, int src) = 0;

    WorkCompletion* ISend(void* sendaddr, uint64_t size_in_bytes, int dest) {
        Buffer sendbuf(sendaddr, size_in_bytes);
        return this->ISend(sendbuf, dest);
    }

    WorkCompletion* IRecv(void* recvaddr, uint64_t size_in_bytes, int src) {
        Buffer recvbuf(recvaddr, size_in_bytes);
        return this->IRecv(recvbuf, src);
    }

    void Barrier();
    /*!
     * @brief performs in-place Allreduce, on sendrecvbuf
     *        this function is NOT thread-safe
     * @param sendrecvbuf_ buffer for both sending and receiving data
     * @param type_nbytes the number of bytes the type has
     * @param count number of elements to be reduced
     * @param reducer reduce function
     */
    void Allreduce(Buffer sendrecvbuf, ReduceFunction reducer) {
    }
    /*!
     * @brief broadcasts data from root to every other node
     * @param sendrecvbuf_ buffer for both sending and receiving data
     * @param size the size of the data to be broadcasted
     * @param root the root worker id to broadcast the data
     */
    void Broadcast(Buffer sendrecvbuf, int root) {
    }

    void Broadcast(void* sendrecvaddr, uint64_t size, int root) {
        Buffer sendrecvbuf(sendrecvaddr, size);
        Broadcast(sendrecvbuf, root);
    }
    void Allgather(std::vector<Buffer> sendrecvbufs) {
    }

    void Allgather(std::vector<void*> sendrecvbufs_,
                   std::vector<uint64_t> sizes) {
        auto num_bufs = sendrecvbufs_.size();
        std::vector<Buffer> sendrecvbufs(num_bufs);
        for (auto i = 0U; i < num_bufs; i++) {
            sendrecvbufs[i].set_addr(sendrecvbufs_[i]);
            sendrecvbufs[i].set_size_in_bytes(sizes[i]);
        }
        Allgather(sendrecvbufs);
    }
    /*! @brief gets rank of current node */
    int GetRank() const;
    /*! @brief gets total number of nodes */
    int GetWorldSize() const;
    /*! @brief whether we run in distribted mode */
    bool IsDistributed() const;
    /*! @brief gets the host name of the current node */
    std::string GetHost() const;
    /*!
     * @brief create a group communicator under this communicator
     * @param ranks ranks of node in this group
     * @param a unique name for this group
     */
    std::unique_ptr<ICommunicator> CreateGroup(const std::vector<int>& ranks,
                                               const std::string& group_name);

    std::string name() const {
        return name_;
    }

    void set_name(const std::string& name) {
        this->name_ = name;
    }

    //! @brief communicator name
    std::string name_;
};

/*!
 * @brief perform in-place Allreduce, on sendrecvbuf
 *   this is an internal function used by rdc to be able to compile with MPI
 *   do not use this function directly
 * @param sendrecvbuf buffer for both sending and receiving data
 * @param reducer reduce function
 * @param dtype the data type
 * @param op the reduce operator type
 */
void Allreduce_(Buffer sendrecvbuf, ReduceFunction red, mpi::DataType dtype,
                mpi::OpType op, const std::string& comm_name);
}  // namespace comm
}  // namespace rdc
