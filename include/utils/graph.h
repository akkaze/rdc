#pragma once
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace rdc {
namespace graph {
template <class Node>
class UndirectedGraph {
public:
    UndirectedGraph() = default;
    ~UndirectedGraph() = default;
    UndirectedGraph(const std::vector<Node>& nodes,
                    const std::vector<std::pair<Node, Node>>& edges)
        : nodes_(nodes), edges_(edges) {
        _BuildAdjacentList();
    }
    UndirectedGraph(std::vector<Node>&& nodes,
                    std::vector<std::pair<Node, Node>>&& edges)
        : nodes_(std::move(nodes)), edges_(std::move(edges)) {
        _BuildAdjacentList();
    }
    UndirectedGraph(const UndirectedGraph& other) = default;
    UndirectedGraph(UndirectedGraph&& other) = default;

    UndirectedGraph& operator=(const UndirectedGraph& other) = default;

    void Create(const std::vector<Node>& nodes,
                const std::vector<std::pair<Node, Node>>& edges) {
        nodes_ = nodes;
        edges_ = edges;
        _BuildAdjacentList();
    }
    void AddEdge(const Node& from, const Node& to) {
        edges_.emplace_back({from, to});
        adjacent_list_[from].emplace(to);
        adjacent_list_[to].emplace(from);
    }
    std::unordered_set<Node> GetNeighbors(const Node& node) {
        return adjacent_list_[node];
    }
    using DistanceDict = std::unordered_map<uint32_t, std::unordered_set<Node>>;
    using NodeDists = std::unordered_map<Node, uint32_t>;
    NodeDists ShortestDist(const Node& from) {
        NodeDists node_distances;
        std::unordered_map<Node, bool> visited;
        for (const auto& node : nodes_) {
            visited[node] = false;
        }
        std::queue<Node> cand;
        cand.emplace(from);
        visited[from] = true;
        node_distances[from] = 0;
        while (!cand.empty()) {
            const auto& cur_node = cand.front();
            cand.pop();
            for (const auto& adj_node : adjacent_list_[cur_node]) {
                if (!visited[adj_node]) {
                    visited[adj_node] = true;
                    node_distances[adj_node] = node_distances[cur_node] + 1;
                    cand.emplace(adj_node);
                }
            }
        }
        return node_distances;
    }

protected:
    void _BuildAdjacentList() {
        for (const auto& e : edges_) {
            const auto& from_node = e.first;
            const auto& to_node = e.second;
            auto& from_adjacents = adjacent_list_[from_node];
            if (!from_adjacents.count(to_node)) {
                from_adjacents.emplace(to_node);
            }
            auto& to_adjacents = adjacent_list_[to_node];
            if (!to_adjacents.count(from_node)) {
                to_adjacents.emplace(to_node);
            }
        }
    }

private:
    std::vector<Node> nodes_;
    std::vector<std::pair<Node, Node>> edges_;
    std::unordered_map<Node, std::unordered_set<Node>> adjacent_list_;
};
}  // namespace graph
}  // namespace rdc
