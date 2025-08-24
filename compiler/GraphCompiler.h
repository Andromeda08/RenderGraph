#pragma once

#include <expected>
#include <ranges>

#include "RenderGraph.h"

enum class RGCompilerError
{
    None,
    NoRootNode,
    CyclicDependency,
    NoNodeByGivenId,
};

struct RGCompilerOutput
{
    bool                     hasFailed  = false;
    RGCompilerError          failReason = RGCompilerError::None;
};

template <class T>
using RGCompilerResult = std::expected<T, RGCompilerError>;

using Id_t = int32_t;

#define rg_CHECK_COMPILER_STEP_RESULT(compilerResult) \
    if (!compilerResult.has_value()) return createErrorOutput(compilerResult)

class RenderGraphCompiler
{
public:
    explicit RenderGraphCompiler(RenderGraph* renderGraph) : mRenderGraph(renderGraph) {}

    RGCompilerOutput compile() const
    {
        RGCompilerOutput output {};

        const auto cullNodesResult = cullNodes();
        rg_CHECK_COMPILER_STEP_RESULT(cullNodesResult);

        const auto serialExecutionOrderResult = getSerialExecutionOrder(cullNodesResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(serialExecutionOrderResult);

        auto parallelizableTasksResult = getParallelizableTasks(serialExecutionOrderResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(parallelizableTasksResult);

        const auto parallelExecutionOrderResult = getParallelExecutionOrder(serialExecutionOrderResult.value(), parallelizableTasksResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(parallelExecutionOrderResult);

        int32_t i = 1;
        for (const auto& task : parallelExecutionOrderResult.value())
        {
            std::cout << std::format("Task #{} > ", i);
            for (const auto& nodeId : task)
            {
                std::cout << mRenderGraph->getPassById(nodeId)->name << ", ";
            }
            std::cout << std::endl;
            i++;
        }

        return output;
    }

    /** Render Graph Compiler : Step 1
     * Cull unreachable nodes from the Render Graph unless they are flagged as "neverCull".
     * @return List of Node IDs that remain after culling.
     */
    RGCompilerResult<std::vector<Id_t>> cullNodes() const noexcept
    {
        const auto rootNode = getRootNode(*mRenderGraph);
        if (!rootNode.has_value())
        {
            return std::unexpected(rootNode.error());
        }

        std::set<Id_t> remainingNodes = mRenderGraph->mVertices
            | std::views::filter([](const Pass* pass){ return pass->flags.neverCull; })
            | std::views::transform([](const Pass* pass){ return pass->mId; })
            | std::ranges::to<std::set<Id_t>>();

        const std::set<Id_t> reachableNodes = BFS::execute(rootNode.value());
        remainingNodes.insert_range(reachableNodes);

        return std::ranges::to<std::vector<Id_t>>(remainingNodes);
    }

    /** Render Graph Compiler : Step 2
     * Get the serial execution order of the remaining nodes.
     * @return List of Node IDs in execution order.
     */
    RGCompilerResult<std::vector<Id_t>> getSerialExecutionOrder(const std::vector<Id_t>& nodeIds) const noexcept
    {
        try {
            const auto nodePtrs = toNodePtrList(nodeIds) | std::ranges::to<std::vector<Vertex*>>();
            return TopologicalSort(nodePtrs).execute();
        } catch (const std::runtime_error&) {
            return std::unexpected(RGCompilerError::CyclicDependency);
        }
    }

    /** Render Graph Compiler : Step 3.1
     * Find parallelizable tasks in the Render Graphs.
     * @param nodeIds List of node IDs in serial execution order.
     * @return Node ID -> List of Node IDs that can run in parallel with the key.
     */
    RGCompilerResult<std::map<int32_t, std::set<Id_t>>> getParallelizableTasks(const std::vector<Id_t>& nodeIds) const noexcept
    {
        const auto nodes = toNodePtrList(nodeIds);
        std::map<Id_t, std::vector<Id_t>> canRunInParallel;

        // Create shadow graph without duplicate edges between nodes
        std::vector<Edge> duplicateEdges;
        for (const auto& edge : mRenderGraph->mEdges)
        {
            const auto duplicated = mRenderGraph->mEdges
                | std::views::filter([edge](const Edge& e) {
                    return edge.id != e.id
                        && edge.src->mId == e.src->mId
                        && edge.dst->mId == e.src->mId;
                })
                | std::ranges::to<std::vector<Edge>>();
            duplicateEdges.append_range(duplicated);
        }

        RenderGraph* shadowGraph = createExampleGraph();
        for (const auto& edge : duplicateEdges)
        {
            shadowGraph->deleteEdge(edge);
        }

        // Propagate dependencies with new edges
        for (auto* node : shadowGraph->mVertices)
        {
            for (auto* dst : shadowGraph->mVertices)
            {
                if (node->mId != dst->mId && BFS::hasPath(node, dst))
                {
                    shadowGraph->insertEdge(node, node->dependencies[0].name, dst, dst->dependencies[0].name);
                }
            }
        }

        // Find parallelizable nodes
        int32_t i = 0;
        for (auto const* node : nodes)
        {
            if (node->flags.sentinel)
            {
                continue;
            }
            std::cout << node->name << " > ";

            std::vector<Id_t> independentNodes;
            int32_t j = 0;
            for (auto const* other : nodes | std::views::filter([&node](const Pass* p){ return p->mId != node->mId; }))
            {
                if (!other->flags.sentinel && i < j && !shadowGraph->containsAnyEdge(node, other))
                {
                    independentNodes.push_back(other->mId);
                    std::cout << other->name << ", ";
                }
                j++;
            }
            std::cout << std::endl;
            canRunInParallel[node->mId] = independentNodes;
            i++;
        }

        std::map<int32_t, std::set<Id_t>> result;
        for (const auto& [key, value] : canRunInParallel)
        {
            for (auto& v : value)
            {
                if (const auto index = indexOfNode(key, nodeIds);
                    index.has_value())
                {
                    result[index.value()] = { key, v };
                }
            }
        }

        delete shadowGraph;
        return result;
    }

    /** Render Graph Compiler : Step 3.2
     *
     */
    static RGCompilerResult<std::vector<std::vector<Id_t>>> getParallelExecutionOrder(
        const std::vector<Id_t>&           serialExecutionOrder,
        std::map<int32_t, std::set<Id_t>>& parallelizableTasks) noexcept
    {
        std::vector<std::vector<Id_t>> result;

        std::set<Id_t> processed;
        for (int32_t i = 0; i < serialExecutionOrder.size(); i++)
        {
            const Id_t nodeId = serialExecutionOrder[i];
            if (!parallelizableTasks.contains(i))
            {
                result.push_back({ nodeId });
                processed.insert(nodeId);
                continue;
            }

            std::vector<Id_t> task;
            task.append_range(parallelizableTasks[i]);
            processed.insert_range(parallelizableTasks[i]);
            result.push_back(task);
        }

        return result;
    }

    template <class T>
    static RGCompilerOutput createErrorOutput(const RGCompilerResult<T>& result)
    {
        RGCompilerOutput output;
        output.hasFailed = true;
        output.failReason = result.error();
        return output;
    }

    /**
     * Transform a list of Node IDs to a list of Node Pointers. (No exists check)
     */
    std::vector<Pass*> toNodePtrList(const std::vector<Id_t>& nodeIds) const noexcept
    {
        return nodeIds
            | std::views::transform([this](const int32_t id){ return mRenderGraph->getPassById(id); })
            | std::ranges::to<std::vector<Pass*>>();
    }

    /**
     * Transform a list of Node Pointers to a list of Node IDs.
     */
    static std::vector<Id_t> toNodeIdList(const std::vector<Pass*>& nodes) noexcept
    {
        return nodes
            | std::views::transform([](const Pass* pass){ return pass->mId; })
            | std::ranges::to<std::vector<Id_t>>();
    }

    static RGCompilerResult<int32_t> indexOfNode(const Id_t nodeId, const std::vector<Id_t>& nodeIds) noexcept
    {
        const auto find = std::ranges::find_if(nodeIds, [&nodeId](const Id_t& id){ return nodeId == id; });
        if (find == std::end(nodeIds))
        {
            return std::unexpected(RGCompilerError::NoNodeByGivenId);
        }
        return std::distance(std::begin(nodeIds), find);
    }

private:
    static RGCompilerResult<Pass*> getRootNode(RenderGraph& renderGraph)
    {
        const auto beginPass = std::ranges::find_if(renderGraph.mVertices,[](const Pass* pass) {
            return pass->flags.sentinel && pass->name == rgRootPassName;
        });

        if (beginPass == std::end(renderGraph.mVertices))
        {
            return std::unexpected(RGCompilerError::NoRootNode);
        }
        return *beginPass;
    }

    RenderGraph* mRenderGraph {nullptr};
};
