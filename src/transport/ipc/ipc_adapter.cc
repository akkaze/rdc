#include "transport/ipc/ipc_adapter.h"
#include "comm/tracker.h"
#include "transport/ipc/ipc_channel.h"

namespace rdc {
IpcAdapter::IpcAdapter() {
    ComputePrivateDataLocations();
    auto&& rank = comm::Tracker::Get()->rank();
    priv_data_.reset(new Shmem(std::to_string(rank),
                               priv_data_locs_.size() * kMemFileNameSize));
    std::memset(priv_data_->Data(), 0,
                priv_data_locs_.size() * kMemFileNameSize);
}

IpcAdapter* IpcAdapter::Get() {
    static IpcAdapter adapter;
    return &adapter;
}

void IpcAdapter::SetPrivData(int rank, int peer_rank,
                             char priv_data[kMemFileNameSize]) {
    int loc = 0;
    if (rank < peer_rank) {
        loc = priv_data_locs_[std::make_tuple(rank, peer_rank)];
    } else {
        loc = priv_data_locs_[std::make_tuple(peer_rank, rank)];
    }
    std::memcpy(priv_data_->Data() + loc * kMemFileNameSize, priv_data,
                kMemFileNameSize);
}

void IpcAdapter::GetPrivData(int rank, int peer_rank,
                             char priv_data[kMemFileNameSize]) {
    int loc = 0;
    if (rank < peer_rank) {
        loc = priv_data_locs_[std::make_tuple(rank, peer_rank)];
    } else {
        loc = priv_data_locs_[std::make_tuple(peer_rank, rank)];
    }
    std::memcpy(priv_data, priv_data_->Data() + loc * kMemFileNameSize,
                kMemFileNameSize);
}

void IpcAdapter::ComputePrivateDataLocations() {
    int loc = 0;
    auto&& peers_with_same_host = comm::Tracker::Get()->peers_with_same_host();
    for (auto&& i = 0U; i < peers_with_same_host.size(); i++) {
        for (auto&& j = i + 1; j < peers_with_same_host.size(); j++) {
            auto&& peer_with_same_host = peers_with_same_host[i];
            auto&& peer_with_same_host2 = peers_with_same_host[j];
            priv_data_locs_[std::make_tuple(peer_with_same_host,
                                            peer_with_same_host2)] = loc++;
        }
    }
}

void IpcAdapter::Listen(const int& port) {
    return;
}

IChannel* Accept() {
    auto channel = new IpcChannel();
    return channel;
}
}  // namespace rdc
