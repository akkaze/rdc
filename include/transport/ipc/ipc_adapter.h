#pragma once
#include <algorithm>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>
#include "transport/adapter.h"
#include "transport/channel.h"
#include "transport/ipc/ipc_channel.h"
#include "utils/lock_utils.h"

bool operator==(const std::tuple<int, int>& lhs,
                const std::tuple<int, int>& rhs) {
    return (std::get<0>(lhs) == std::get<0>(rhs)) &&
           (std::get<1>(lhs) == std::get<1>(rhs));
}

namespace std {
template <>
struct hash<std::tuple<int, int>> {
    typedef std::tuple<int, int> argument_type;
    typedef std::size_t result_type;
    result_type operator()(argument_type const& s) const noexcept {
        result_type const h1(std::hash<int>{}(std::get<0>(s)));
        result_type const h2(std::hash<int>{}(std::get<1>(s)));
        return h1 ^ (h2 << 1);  // or use boost::hash_combine
    }
};
}  // namespace std

namespace rdc {
/**
 * @class IpcAdapter
 * @brief tcpadapther which will govern all tcp connections
 */
class IpcAdapter : public IAdapter {
public:
    IpcAdapter();
    static IpcAdapter* Get();

    ~IpcAdapter();

    void Shutdown();

    void Listen(const int& port) override;

    IChannel* Accept() override;

    void SetPrivData(int rank, int peer_rank,
                     char priv_data[kMemFileNameSize]);

    void GetPrivData(int rank, int peer_rank,
                     char priv_data[kMemFileNameSize]);

    void ComputePrivateDataLocations();

private:
    std::unordered_map<std::tuple<int, int>, int> priv_data_locs_;
    std::unique_ptr<Shmem> priv_data_;
};

}  // namespace rdc
