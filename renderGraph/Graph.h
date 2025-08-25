#pragma once

#include <cstdint>
#include <expected>
#include <set>
#include <vector>

struct Vertex
{
    virtual ~Vertex() = default;

    int32_t              mId = -1;
    std::vector<Vertex*> mIncomingEdges;
    std::vector<Vertex*> mOutgoingEdges;
};

// BFS and algorithms based on it.
struct BFS
{
    /**
     * @return Set of vertex IDs which were visited during execution.
     */
    static std::set<int32_t> execute(Vertex* root);

    /**
     * @return Does a path exist from A to B
     */
    static bool hasPath(Vertex* src, Vertex* dst);
};

// Topological Sort for directed (acyclic) graphs.
struct TopologicalSort
{
    enum class Error
    {
        GraphNotAcyclic,
    };

    /**
     * @return List of Vertex IDs in topological order.
     */
    static std::expected<std::vector<int32_t>, Error> execute(const std::vector<Vertex*>& vertices) noexcept;
};
