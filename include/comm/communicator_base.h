/*!
 *  Copyright (c) 2018 by Contributors
 * \file allreduce_base.h
 * @brief Basic implementation of communication primitives
 *   include non-block send recv, allreduce, broadcast and allgather
 *   using TCP non-block socket or RDMA for communication.
 *
 *   This implementation provides basic utility of Communication Primitives
 *   without considering node failure
 *
 * \author Ankun Zheng
 */
#pragma once

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "comm/communicator.h"
#include "comm/deamon.h"
#include "common/status.h"
#include "core/logging.h"
#include "core/work_request.h"
#include "transport/adapter.h"
#include "transport/channel.h"
#include "transport/tcp/socket.h"
#include "transport/tcp/tcp_adapter.h"
#include "transport/tcp/tcp_channel.h"
#include "utils/graph.h"
#include "utils/utils.h"
#ifdef RDC_USE_RDMA
#include "transport/rdma/rdma_adapter.h"
#include "transport/rdma/rdma_channel.h"
#endif
#include "comm/tracker.h"
#include "core/mpi.h"
namespace rdc {
namespace comm {
/*! @brief implementation of basic Allreduce comm */
class Communicator : public ICommunicator {
public:
    Communicator();
    Communicator(const std::string& name);
    Communicator(const Communicator& other);
    ~Communicator() override;
    void Init(int world_size, int num_conn, int num_accept);
    /*! @brief shutdown the comm */
    void Shutdown() override;
    void ResetLinks() override;
    /**
     * @brief:
     *
     * @param world_size
     */
    void BuildTopology(const int32_t& world_size);
    /*!
     * @brief connect to the tracker_ to fix the the missing links
     *   this function is also used when the comm start up
     * @param cmd possible command to sent to tracker_
     */
    void ReConnectLinks(const std::tuple<int, int>& num_conn_accept);

    /*! @brief get rank */
    int GetRank() const {
        auto rank = Tracker::Get()->rank();
        return rank;
    }
    int GetPrevRank() const {
        auto&& rank = GetRank();
        auto&& world_size = GetWorldSize();
        auto&& prev_rank = (rank - 1) % world_size;
        return prev_rank;
    }

    int GetNextRank() const {
        auto&& rank = GetRank();
        auto&& world_size = GetWorldSize();
        auto&& next_rank = (rank + 1) % world_size;
        return next_rank;
    }
    /*! @brief get world size */
    int GetWorldSize() const {
        auto world_size = Tracker::Get()->world_size();
        if (world_size == -1)
            return 1;
        return world_size;
    }
    /*! @brief whether is distributed or not */
    bool IsDistributed() const {
        auto&& tracker_uri = Tracker::Get()->host_uri();
        return tracker_uri != "NULL";
    }

    /*! @brief get host uri */
    std::string GetHost(void) const {
        auto host_uri = Tracker::Get()->host_uri();
        return host_uri;
    }
    /*!
     *  @brief blocking send
     *  @param sendbuf_ buffer need to  send
     *  @param nbytes buffer size in bytes
     *  @param dest destination rank
     */
    void Send(Buffer sendbuf_, int dest) override;
    /*!
     *  @brief blocking send
     *  @param sendbuf_ buffer need to  send
     *  @param nbytes buffer size in bytes
     *  @param dest destination rank
     */
    void Recv(Buffer recvbuf_, int src);

    WorkCompletion* ISend(Buffer sendbuf_, int dest);

    WorkCompletion* IRecv(Buffer recvbuf_, int src);
    /*! @brief barrier all nodes*/
    void Barrier();
    /*! @brief exclude communications with tracker by other communicator*/
    void Exclude();
    /*! @brief unexclude communications with tracker by other communicator*/
    void UnExclude();
    /*! @brief register this communicator to tracker */
    void Register();
    /*!
     * @brief perform in-place allreduce, on sendrecvbuf
     *        this function is NOT thread-safe
     * @param sendrecvbuf_ buffer for both sending and recving data
     * @param type_nbytes the unit number of bytes the type have
     * @param count number of elements to be reduced
     * @param reducer reduce function
     */
    void Allreduce(Buffer sendrecvbuf_, ReduceFunction reducer) {
        if (GetWorldSize() == 1 || GetWorldSize() == -1) {
            return;
        }
        TryAllreduce(sendrecvbuf_, reducer);
    }
    /*!
     * @brief broadcast data from root to all nodes
     * @param sendrecvbuf_ buffer for both sending and recving data
     * @param size the size of the data to be broadcasted
     * @param root the root worker id to broadcast the data
     */
    void Broadcast(Buffer sendrecvbuf_, int root) {
        if (GetWorldSize() == 1 || GetWorldSize() == -1)
            return;
        TryBroadcast(sendrecvbuf_, root);
    }

    void Allgather(std::vector<Buffer> sendrecvbufs_) {
        if (GetWorldSize() == 1 || GetWorldSize() == -1)
            return;
        TryAllgatherRing(sendrecvbufs_);
    }
    std::unique_ptr<ICommunicator> CreateGroup(const std::vector<int>& ranks,
                                               const std::string& group_name);

protected:
    /*!
     * @brief perform in-place allreduce, on sendrecvbuf, this function can
     * fail, and will return the cause of failure
     *
     * @param sendrecvbuf_ buffer for both sending and recving data
     * @param reducer reduce function
     * @return this function can return Status::kSuccess, kSockError,
     * kGetExcept, see void for details
     */
    void TryAllreduce(Buffer sendrecvbuf_, ReduceFunction reducer);

    void TryReduceTree(Buffer sendrecvbuf_, Buffer reducebuf_,
                       ReduceFunction reducer, int root);
    /*!
     * @brief broadcast data from root to all nodes, this function can fail,and
     *  will return the cause of failure
     * @param sendrecvbuf_ buffer for both
     * sending and receiving data
     * @param size the size of the data to be
     * broadcasted
     * @param root the root worker id to broadcast the data @return
     * this function can return Status::kSuccess, kSockError, kGetExcept, see
     * void for details
     * @sa void
     */
    void TryBroadcast(Buffer sendrecvbuf_, int root);

    /*!
     * @brief perform in-place allreduce, on sendrecvbuf,
     * this function implements tree-shape reduction
     *
     * @param sendrecvbuf_ buffer for both sending and recving data
     * @param type_nbytes the unit number of bytes the type have
     * @param count number of elements to be reduced
     * @param reducer reduce function
     * @return this function can return Status::kSuccess, kSockError,
     * kGetExcept, see void for details \sa void
     */
    void TryAllreduceTree(Buffer sendrecvbuf_, ReduceFunction reducer);
    /*!
     * @brief internal Allgather function, each node have a segment of data in
     * the ring of sendrecvbuf, the data provided by current node k is
     * [slice_begin, slice_end), the next node's segment must start with
     * slice_end after the call of Allgather, sendrecvbuf_ contains all the
     * contents including all segments use a ring based algorithm
     *
     * @param sendrecvbufs_ buffers for both sending and receiving data, each
     * node holds one chunk of
     * @buffer at begin, buffers will be passed in the
     * ring
     * @param type_nbytes the unit number of bytes the type have
     * @param count counts of type hold in buffers
     * @return this function can return
     * Status::kSuccess, kSockError, kGetExcept, see void for details
     * @sa void
     */
    void TryAllgatherRing(std::vector<Buffer> sendrecvbufs_);
    /*!
     * @brief perform in-place allreduce, reduce on the sendrecvbuf,
     *
     *  after the function, node k get k-th segment of the reduction result
     *  the k-th segment is defined by [k * step, min((k + 1) * step,count) )
     *  where step = ceil(count / GetWorldSize())
     *
     * @param sendrecvbuf_ buffer for both sending and recving data
     * @param reducebuf_ buffer for reducing data
     * @param type_nbytes the unit number of bytes the type have
     * @param count number of elements to be reduced
     * @param reducer reduce function
     * @return this function can return Status, see void for details
     * @sa void, TryAllreduce
     */
    void TryReduceScatterRing(Buffer sendrecvbuf_, Buffer reducebuf_,
                              ReduceFunction reducer);
    /*!
     * @brief perform in-place allreduce, on sendrecvbuf
     *  use a ring based algorithm, reduce-scatter + allgather
     *
     * @param sendrecvbuf_ buffer for both sending and recving data
     * @param type_nbytes the unit number of bytes the type have
     * @param count number of elements to be reduced
     * @param reducer reduce function
     * @return this function can return Status see void for details
     * @sa void
     */
    void TryAllreduceRing(Buffer sendrecvbuf_, ReduceFunction reducer);

    bool is_main_comm() const {
        return this->is_main_comm_;
    }

    //---- data structure related to model ----
    int version_number;
    std::mutex conn_lock_;
    //---- local data related to link ----
    // rank of parent node, can be -1
    int parent_rank_;
    // channels of all links referenced by rank
    std::unordered_map<int, std::shared_ptr<IChannel>> all_links_;
    // used to record the link where things goes wrong
    IChannel* err_link;
    graph::UndirectedGraph<int> tree_map_;
    // all the links in the reduction tree connection
    std::vector<IChannel*> tree_links;
    // the rank of neighbors
    std::map<int, int> tree_neighbors_;
    int num_neighbors_;
    // pointer to links in the ring
    IChannel *ring_prev_, *ring_next_;
    int prev_rank_, next_rank_;
    //----- meta information-----
    // unique identifier of the possible job this process is doing
    // reduction method
    int reduce_method;
    // children communicators
    std::unordered_map<uint32_t, Communicator*> groups_;
    // children counter
    uint32_t children_counter_;
    std::mutex comm_lock_;
    std::unordered_map<std::string, std::unique_ptr<Communicator>> sub_comms_;
    bool is_main_comm_;
};
}  // namespace comm
}  // namespace rdc
