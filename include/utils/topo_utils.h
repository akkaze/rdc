#pragma once
#include <algorithm>
#include <cassert>
#include <vector>
#include <unordered_map>
namespace rdc {
std::vector<int> GetNeighbors(const int& rank, const uint32_t& num_workers);

std::tuple<std::unordered_map<int, std::vector<int>>,
           std::unordered_map<int, int>>
GetTree(const uint32_t& num_workers);

std::vector<int> FindShareRing(
    std::unordered_map<int, std::vector<int>> tree_map,
    std::unordered_map<int, int> parent_map, const int& rank);

std::unordered_map<int, std::pair<int, int>> GetRing(
    std::unordered_map<int, std::vector<int>> tree_map,
    std::unordered_map<int, int> parent_map);

std::tuple<std::unordered_map<int, std::vector<int>>,
           std::unordered_map<int, int>,
           std::unordered_map<int, std::pair<int, int>>>
GetLinkMap(const uint32_t& num_workers);
}  // namespace rdc
