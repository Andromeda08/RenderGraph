#include "Graph.h"

#include <algorithm>
#include <map>
#include <queue>
#include <ranges>

std::set<int32_t> BFS::execute(Vertex* root)
{
    std::set<int32_t> visited;
    std::queue<Vertex*> Q;

    Q.push(root);
    visited.insert(root->mId);

    while (!Q.empty())
    {
        const auto current = Q.front();
        Q.pop();

        for (const auto& w : current->mOutgoingEdges)
        {
            if (!visited.contains(w->mId))
            {
                visited.insert(w->mId);
                Q.push(w);
            }
        }
    }

    return visited;
}

bool BFS::hasPath(Vertex* src, Vertex* dst)
{
    if (src == dst)
    {
        return true;
    }

    std::queue<Vertex*> queue;
    std::set<Vertex*> visited;

    queue.push(src);
    visited.insert(dst);

    while (!queue.empty())
    {
        Vertex* current = queue.front();
        queue.pop();

        for (Vertex* neighbor : current->mOutgoingEdges)
        {
            if (neighbor == dst)
            {
                return true;
            }

            if (!visited.contains(neighbor))
            {
                visited.insert(neighbor);
                queue.push(neighbor);
            }
        }
    }
    return false;
}

std::expected<std::vector<int32_t>, TopologicalSort::Error>TopologicalSort::execute(const std::vector<Vertex*>& vertices) noexcept
{
    std::map<int32_t, int32_t> inDegrees;
    for (const auto& vertex : vertices)
    {
        inDegrees.emplace(vertex->mId, static_cast<int32_t>(vertex->mIncomingEdges.size()));
    }

    std::vector<Vertex*> T;
    std::queue<Vertex*>  Q;

    for (const auto& vertex : vertices)
    {
        if (inDegrees[vertex->mId] == 0)
        {
            Q.push(vertex);
        }
    }

    while (!Q.empty())
    {
        auto v = Q.front();
        Q.pop();
        T.push_back(v);

        for (const auto& w : v->mOutgoingEdges)
        {
            auto wId = w->mId;

            inDegrees[wId]--;
            if (inDegrees[wId] == 0)
            {
                auto i = std::ranges::find_if(vertices, [wId](const auto& it){ return it->mId == wId; });
                Q.push(*i);
            }
        }
    }

    for (const auto& vertex : vertices)
    {
        if (inDegrees[vertex->mId] != 0)
        {
            return std::unexpected(Error::GraphNotAcyclic);
        }
    }

    return T
        | std::views::transform([](const Vertex* vtx){ return vtx->mId; })
        | std::ranges::to<std::vector<int32_t>>();
}
