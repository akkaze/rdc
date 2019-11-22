#include "comm/communicator_base.h"
#include "comm/communicator_manager.h"

namespace rdc {
namespace comm {
void Communicator::TryAllreduce(Buffer sendrecvbuf, ReduceFunction reducer) {
    if (sendrecvbuf.size_in_bytes() >
        CommunicatorManager::Get()->reduce_ring_mincount()) {
        return this->TryAllreduceRing(sendrecvbuf, reducer);
    } else {
        return this->TryAllreduceTree(sendrecvbuf, reducer);
    }
}
void Communicator::TryReduceTree(Buffer sendrecvbuf, Buffer reducebuf,
                                 ReduceFunction reducer, int root) {
    auto dists_from_root = tree_map_.ShortestDist(root);
    auto dist_from_root = dists_from_root[GetRank()];
    auto neighbors = tree_map_.GetNeighbors(GetRank());
    std::unordered_set<int> recv_from_nodes;
    int send_to_node = -1;
    for (const auto& neighbor : neighbors) {
        if (dists_from_root[neighbor] == dist_from_root + 1) {
            recv_from_nodes.insert(neighbor);
        } else if (dists_from_root[neighbor] == dist_from_root - 1) {
            send_to_node = neighbor;
        }
    }
    for (const auto& recv_from_node : recv_from_nodes) {
        auto wc = all_links_[recv_from_node]->IRecv(reducebuf);
        wc->Wait();
        reducer(reducebuf, sendrecvbuf);
        WorkCompletion::Delete(wc);
    }

    auto chain_wc = ChainWorkCompletion::New();
    if (send_to_node != -1) {
        auto wc = all_links_[send_to_node]->ISend(sendrecvbuf);
        chain_wc->Add(wc);
    }
    chain_wc->Wait();
    ChainWorkCompletion::Delete(chain_wc);
    return;
}
void Communicator::TryBroadcast(Buffer sendrecvbuf, int root) {
    auto dists_from_root = tree_map_.ShortestDist(root);
    auto dist_from_root = dists_from_root[GetRank()];
    auto neighbors = tree_map_.GetNeighbors(GetRank());
    std::unordered_set<int> send_to_nodes;
    int recv_from_node = -1;
    for (const auto& neighbor : neighbors) {
        if (dists_from_root[neighbor] == dist_from_root + 1) {
            send_to_nodes.insert(neighbor);
        } else if (dists_from_root[neighbor] == dist_from_root - 1) {
            recv_from_node = neighbor;
        }
    }
    auto chain_wc = ChainWorkCompletion::New();
    if (recv_from_node != -1) {
        auto wc = all_links_[recv_from_node]->IRecv(sendrecvbuf);
        chain_wc->Add(wc);
    }
    for (const auto& send_to_node : send_to_nodes) {
        auto wc = all_links_[send_to_node]->ISend(sendrecvbuf);
        chain_wc->Add(wc);
    }
    chain_wc->Wait();
    ChainWorkCompletion::Delete(chain_wc);
    return;
}

void Communicator::TryAllreduceTree(Buffer sendrecvbuf,
                                    ReduceFunction reducer) {
    Buffer reducebuf(sendrecvbuf.size_in_bytes());
    reducebuf.AllocTemp(utils::AllocTemp);
    TryReduceTree(sendrecvbuf, reducebuf, reducer, 0);
    reducebuf.FreeTemp(utils::Free);
    TryBroadcast(sendrecvbuf, 0);
}
void Communicator::TryAllgatherRing(std::vector<Buffer> sendrecvbufs) {
    // read from next link and send to prev one
    auto &prev = ring_prev_, &next = ring_next_;
    const size_t count_bufs = GetWorldSize();
    const size_t stop_write_idx = count_bufs + GetRank() - 1;
    const size_t stop_read_idx = count_bufs + GetRank();
    size_t write_idx = GetRank();
    size_t read_idx = GetRank() + 1;
    while (true) {
        bool finished = true;
        if (read_idx != stop_read_idx) {
            finished = false;
        }
        if (write_idx != stop_write_idx) {
            finished = false;
        }
        if (finished)
            break;
        auto chain_wc = ChainWorkCompletion::New();
        if (write_idx < read_idx && write_idx != stop_write_idx) {
            size_t start = write_idx % count_bufs;
            auto wc = prev->ISend(sendrecvbufs[start]);
            chain_wc->Add(wc);
            write_idx++;
        }
        if (read_idx != stop_read_idx) {
            size_t start = read_idx % count_bufs;
            auto wc = next->IRecv(sendrecvbufs[start]);
            chain_wc->Add(wc);
            //            wc.Wait();
            read_idx++;
        }
        chain_wc->Wait();
        ChainWorkCompletion::Delete(chain_wc);
    }
}
void Communicator::TryReduceScatterRing(Buffer sendrecvbuf, Buffer reducebuf,
                                        ReduceFunction reducer) {
    // read from next link and send to prev one
    auto &&prev = ring_prev_, &&next = ring_next_;
    uint64_t n = static_cast<uint64_t>(GetWorldSize());
    const auto& ranges = utils::Split(0, sendrecvbuf.Count(), n);
    uint64_t write_idx = GetNextRank();
    uint64_t read_idx = GetNextRank() + 1;
    uint64_t reduce_idx = read_idx;
    // position to stop reading
    const uint64_t stop_read_idx = n + GetNextRank();
    // position to stop writing
    size_t stop_write_idx = n + GetRank();
    ;
    const auto& item_size = sendrecvbuf.item_size();
    if (stop_write_idx > stop_read_idx) {
        stop_write_idx -= n;
        CHECK_F(write_idx <= stop_write_idx, "write ptr boundary check");
    }
    while (true) {
        bool finished = true;
        if (read_idx != stop_read_idx) {
            finished = false;
        }
        if (write_idx != stop_write_idx) {
            finished = false;
        }
        if (finished)
            break;
        auto chain_wc = ChainWorkCompletion::New();
        if (write_idx < reduce_idx && write_idx != stop_write_idx) {
            uint64_t write_pos = write_idx % n;
            uint64_t write_size =
                (ranges[write_pos].second - ranges[write_pos].first) *
                item_size;
            uint64_t write_start = ranges[write_pos].first * item_size;
            auto wc = prev->ISend(
                sendrecvbuf.Slice(write_start, write_start + write_size));
            chain_wc->Add(wc);
            write_idx++;
        }
        if (read_idx != stop_read_idx) {
            uint64_t read_pos = read_idx % n;
            uint64_t read_start = ranges[read_pos].first * item_size;
            uint64_t read_size =
                (ranges[read_pos].second - ranges[read_pos].first) *
                item_size;
            auto wc = next->IRecv(
                reducebuf.Slice(read_start, read_start + read_size));
            chain_wc->Add(wc);
            chain_wc->Wait();
            CHECK_F(read_idx <= stop_read_idx, "[%d] read_ptr boundary check",
                    GetRank());
            read_idx++;
            size_t reduce_pos = reduce_idx % n;
            size_t reduce_start = ranges[reduce_pos].first * item_size;
            size_t reduce_size =
                (ranges[reduce_pos].second - ranges[reduce_pos].first) *
                item_size;
            reducer(
                reducebuf.Slice(reduce_start, reduce_start + reduce_size),
                sendrecvbuf.Slice(reduce_start, reduce_start + reduce_size));
            reduce_idx++;
        }
        ChainWorkCompletion::Delete(chain_wc);
    }
    return;
}
void Communicator::TryAllreduceRing(Buffer sendrecvbuf,
                                    ReduceFunction reducer) {
    Buffer reducebuf(sendrecvbuf.size_in_bytes());
    reducebuf.AllocTemp(utils::AllocTemp);
    reducebuf.set_item_size(sendrecvbuf.item_size());
    TryReduceScatterRing(sendrecvbuf, reducebuf, reducer);
    reducebuf.FreeTemp(utils::Free);
    uint64_t n = static_cast<uint64_t>(GetWorldSize());
    const auto& ranges = utils::Split(0, sendrecvbuf.Count(), n);
    // get rank of previous
    std::vector<Buffer> sendrecvbufs(n);
    for (auto i = 0U; i < n; i++) {
        uint64_t begin = ranges[i].first;
        uint64_t end = ranges[i].second;
        uint64_t size = (end - begin) * sendrecvbuf.item_size();
        sendrecvbufs[i].set_size_in_bytes(size);
        sendrecvbufs[i].set_addr(utils::IncrVoidPtr(
            sendrecvbuf.addr(), begin * sendrecvbuf.item_size()));
    }
    return TryAllgatherRing(sendrecvbufs);
}
}  // namespace comm
}  // namespace rdc
