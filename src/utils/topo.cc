#include "utils/topo_utils.h"
namespace rdc{
std::vector<int> GetNeighbors(const int& rank, const uint32_t& num_workers) {
    auto next = rank + 1;
    std::vector<int> neighbors;
    // parent
    if (next > 1) {
        neighbors.emplace_back(next / 2 - 1);
    }
    // children
    if (next * 2 - 1 < static_cast<int>(num_workers)) {
        neighbors.emplace_back(next * 2 - 1);
    }
    if (next * 2 < static_cast<int>(num_workers)) {
        neighbors.emplace_back(next * 2);
    }
    return neighbors;
}

std::tuple<std::unordered_map<int, std::vector<int>>,
           std::unordered_map<int, int>>
GetTree(const uint32_t& num_workers) {
    std::unordered_map<int, std::vector<int>> tree_map;
    std::unordered_map<int, int> parent_map;
    for (auto r = 0U; r < num_workers; r++) {
        tree_map[r] = GetNeighbors(r, num_workers);
        parent_map[r] = (r + 1) / 2 - 1;
    }
    return std::make_tuple(tree_map, parent_map);
}

std::vector<int> FindShareRing(
    std::unordered_map<int, std::vector<int>> tree_map,
    std::unordered_map<int, int> parent_map, const int& rank) {
    const auto& neighbors = tree_map[rank];
    std::vector<int> children;
    const auto& parent = parent_map[rank];

    std::vector<int> share_ring;
    for (const auto& neighbor : neighbors) {
        if (neighbor != parent) {
            children.emplace_back(neighbor);
        }
    }
    if (children.size() == 0) {
        share_ring.emplace_back(rank);
        return share_ring;
    }
    share_ring.emplace_back(rank);
    uint32_t counter = 0;
    for (const auto& child : children) {
        auto vlist = FindShareRing(tree_map, parent_map, child);
        counter++;
        if (counter == children.size()) {
            std::reverse(vlist.begin(), vlist.end());
        }
        for (const auto& v : vlist) {
            share_ring.emplace_back(v);
        }
    }
    return share_ring;
}

std::unordered_map<int, std::pair<int, int>> GetRing(
    std::unordered_map<int, std::vector<int>> tree_map,
    std::unordered_map<int, int> parent_map) {
    assert(parent_map[0] == -1);
    auto rlist = FindShareRing(tree_map, parent_map, 0);
    assert(rlist.size() == tree_map.size());
    std::unordered_map<int, std::pair<int, int>> ring_map;
    const auto& num_workers = tree_map.size();
    for (auto r = 0U; r < num_workers; r++) {
        auto rprev = (r + num_workers - 1) % num_workers;
        auto rnext = (r + 1) % num_workers;
        ring_map[rlist[r]] = std::make_pair(rlist[rprev], rlist[rnext]);
    }
    return ring_map;
}

std::tuple<std::unordered_map<int, std::vector<int>>,
           std::unordered_map<int, int>,
           std::unordered_map<int, std::pair<int, int>>>
GetLinkMap(const uint32_t& num_workers) {
    auto tree_and_parent_map = GetTree(num_workers);
    auto tree_map = std::get<0>(tree_and_parent_map);
    auto parent_map = std::get<1>(tree_and_parent_map);
    auto ring_map = GetRing(tree_map, parent_map);
    std::unordered_map<int, int> rmap;
    rmap[0] = 0;
    int k = 0;
    for (auto i = 0U; i < num_workers - 1; i++) {
        k = ring_map[k].second;
        rmap[k] = i + 1;
    }
    std::unordered_map<int, std::vector<int>> _tree_map;
    std::unordered_map<int, int> _parent_map;
    std::unordered_map<int, std::pair<int, int>> _ring_map;
    for (auto item : ring_map) {
        _ring_map[rmap[item.first]] =
            std::make_pair(rmap[item.second.first], rmap[item.second.second]);
    }
    for (auto item : tree_map) {
        for (auto x : item.second) {
            _tree_map[rmap[item.first]].emplace_back(rmap[x]);
        }
    }
    for (auto item : parent_map) {
        if (item.first != 0) {
            _parent_map[rmap[item.first]] = rmap[item.second];
        } else {
            _parent_map[rmap[item.first]] = -1;
        }
    }
    return std::make_tuple(_tree_map, _parent_map, _ring_map);
}
}
