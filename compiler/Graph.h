#pragma once

#include <algorithm>
#include <ranges>
#include <queue>
#include <map>
#include <set>
#include <vector>

struct Vertex
{
    virtual ~Vertex() = default;

    int32_t              mId = -1;
    std::vector<Vertex*> mIncomingEdges;
    std::vector<Vertex*> mOutgoingEdges;
};

struct BFS
{
    /**
     * @return Set of vertex IDs which were visited during execution.
     */
    static std::set<int32_t> execute(Vertex* root)
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

    /**
     * @return Does a path exist from A to B
     */
    static bool hasPath(Vertex* src, Vertex* dst)
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
};

class TopologicalSort
{
public:
    explicit TopologicalSort(const std::vector<Vertex*>& vertices)
    : mVertices(vertices)
    {
    }

    [[nodiscard]] std::vector<int32_t> execute() const
    {
        std::map<int32_t, int32_t> in_degrees;
        for (const auto& vertex : mVertices)
        {
            in_degrees.emplace(vertex->mId, vertex->mIncomingEdges.size());
        }

        std::vector<Vertex*> T;
        std::queue<Vertex*>  Q;

        for (const auto& vertex : mVertices)
        {
            if (in_degrees[vertex->mId] == 0)
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
                auto w_id = w->mId;

                in_degrees[w_id]--;
                if (in_degrees[w_id] == 0)
                {
                    auto i = std::ranges::find_if(mVertices, [w_id](const auto& it){ return it->mId == w_id; });
                    Q.push(*i);
                }
            }
        }

        for (const auto& vertex : mVertices)
        {
            if (in_degrees[vertex->mId] != 0)
            {
                throw std::runtime_error("Given graph was not acyclic.");
            }
        }

        return T
            | std::views::transform([](const Vertex* vtx){ return vtx->mId; })
            | std::ranges::to<std::vector<int32_t>>();
    }

private:
    const std::vector<Vertex*>& mVertices;
};