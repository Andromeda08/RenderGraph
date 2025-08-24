#pragma once

#include <expected>
#include <ranges>

#include "RenderGraph.h"

// =======================================
// Enum Types
// =======================================

enum class RGCompilerError
{
    None,
    NoRootNode,
    CyclicDependency,
    NoNodeByGivenId,
};

// =======================================
// Data Types
// =======================================

struct RGCompilerOptions
{
    bool allowParallelization = false;
};

struct RGCompilerOutput
{
    bool                     hasFailed  = false;
    RGCompilerError          failReason = RGCompilerError::None;
};

struct RGTask
{
    Pass* pass       = nullptr;
    Pass* asyncPass  = nullptr;
};

struct RGPassTemplate {};
struct RGResourceTemplate {};
struct RGBarrierTemplate {};
struct RGSyncPointTemplate {};

// =======================================
// Using Defines
// =======================================

template <class T>
using RGCompilerResult = std::expected<T, RGCompilerError>;

using Node_t = Pass*;

// =======================================
// Utility Macros
// =======================================
#define rg_CHECK_COMPILER_STEP_RESULT(compilerResult) \
    if (!compilerResult.has_value()) return createErrorOutput(compilerResult)

// =======================================
// Render Graph Resource Optimizer
// =======================================
class RenderGraphResourceOptimizer
{
public:

private:
};

// =======================================
// Render Graph Compiler
// =======================================
class RenderGraphCompiler
{
public:
    RenderGraphCompiler(RenderGraph* renderGraph, const RGCompilerOptions& compilerOptions)
    : mRenderGraph(renderGraph)
    , mOptions(compilerOptions)
    {
    }

    RGCompilerOutput compile() const
    {
        RGCompilerOutput output {};

        const auto cullNodesResult = cullNodes();
        rg_CHECK_COMPILER_STEP_RESULT(cullNodesResult);

        const auto serialExecutionOrderResult = getSerialExecutionOrder(cullNodesResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(serialExecutionOrderResult);

        auto parallelizableTasksResult = getParallelizableTasks(serialExecutionOrderResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(parallelizableTasksResult);

        const auto finalTaskOrderResult = getFinalTaskOrder(serialExecutionOrderResult.value(), parallelizableTasksResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(finalTaskOrderResult);

        return output;
    }

private:
    // =======================================
    // Render Graph Compiler Phase : Preamble
    // =======================================

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
            | std::views::filter([](const auto& pass){ return pass->flags.neverCull; })
            | std::views::transform([](const auto& pass){ return pass->mId; })
            | std::ranges::to<std::set<Id_t>>();

        const std::set<Id_t> reachableNodes = BFS::execute(rootNode.value());
        remainingNodes.insert_range(reachableNodes);

        return std::ranges::to<std::vector<Id_t>>(remainingNodes);
    }

    // =======================================
    // Render Graph Compiler Phase : Tasks
    // =======================================

    /** Render Graph Compiler : Step 2.1
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

    /** Render Graph Compiler : Step 2.2
     * Find parallelizable tasks in the Render Graphs.
     * @param nodeIds List of node IDs in serial execution order.
     * @return Node ID -> List of Node IDs that can run in parallel with the key.
     */
    RGCompilerResult<std::map<Id_t, std::vector<Id_t>>> getParallelizableTasks(const std::vector<Id_t>& nodeIds) const noexcept
    {
        std::map<Id_t, std::vector<Id_t>> canRunInParallel;

        // Create shadow graph without multi edges between nodes
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

        RenderGraph shadowGraph = RenderGraph::createCopy(*mRenderGraph);
        for (const auto& edge : duplicateEdges)
        {
            shadowGraph.deleteEdge(edge);
        }

        // Propagate dependencies with new edges
        for (auto& node : shadowGraph.mVertices)
        {
            for (auto& dst : shadowGraph.mVertices)
            {
                if (node->mId != dst->mId && BFS::hasPath(node.get(), dst.get()))
                {
                    shadowGraph.insertEdge(node.get(), node->dependencies[0].name, dst.get(), dst->dependencies[0].name);
                }
            }
        }

        // mRenderGraph->dumpDOT("renderGraph");
        // shadowGraph.dumpDOT("shadowGraph");

        // Shadow Graph nodes in Serial execution order.
        const auto shadowNodes = nodeIds
            | std::views::transform([&](const int32_t id){ return shadowGraph.getPassById(id); })
            | std::ranges::to<std::vector<Pass*>>();

        // Find parallelizable nodes
        for (const auto& [i, node] : std::views::enumerate(shadowNodes))
        {
            if (node->flags.sentinel) continue; /* Ignore sentinel pass */

            std::vector<Id_t> independentNodes;
            for (const auto& [j, other] : std::views::enumerate(shadowNodes))
            {
                if (node->mId == other->mId                         /* Ignore self */
                    || other->flags.sentinel                        /* Ignore sentinel pass */
                    || i > j                                        /* if Other precedes Node ignore */
                    || shadowGraph.containsAnyEdge(node, other) /* if any edge between Node and Other ignore */
                ) continue;
                independentNodes.push_back(other->mId);
            }

            canRunInParallel[node->mId] = independentNodes;
        }

        // Remove empty entries
        for (const auto& nodeId : std::views::keys(canRunInParallel) | std::ranges::to<std::vector<Id_t>>())
        {
            if (canRunInParallel[nodeId].empty())
            {
                canRunInParallel.erase(nodeId);
            }
        }

        /* Debug print for "canRunInParallel"
        for (const auto& [k, v] : canRunInParallel)
        {
            std::cout << mRenderGraph->getPassById(k)->name << " > ";
            for (const auto& [i, otherId] : std::views::enumerate(v))
            {
                std::cout << mRenderGraph->getPassById(otherId)->name;
                if (i + 1 < v.size())
                {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
        }
        */

        return canRunInParallel;
    }

    /** Render Graph Compiler : Step 2.3
     * Create final tasks based on serial execution order and parallelizable tasks.
     * @param serialExecutionOrder List of node IDs in serial execution order.
     * @param parallelizableTasks Map of Node ID -> List of Node IDs that can run in parallel with the key.
     * @return Final list of Render Graph Tasks in execution order.
     */
    RGCompilerResult<std::vector<RGTask>> getFinalTaskOrder(
        const std::vector<Id_t>&           serialExecutionOrder,
        std::map<Id_t, std::vector<Id_t>>& parallelizableTasks) const noexcept
    {
        std::vector<RGTask> tasks;

        const auto nodes = toNodePtrList(serialExecutionOrder);

        // Return pure serialized tasks if parallelization is not allowed by options.
        if (!mOptions.allowParallelization)
        {
            for (const auto& node : nodes)
            {
                RGTask basicTask = {
                    .pass      = node,
                    .asyncPass = nullptr,
                };
                tasks.push_back(basicTask);
            }
            return tasks;
        }

        // Create parallel tasks if possible
        const auto     chancesForParallelization = static_cast<int32_t>(parallelizableTasks.size());
        int32_t        parallelTaskCount = 0;
        std::set<Id_t> nodesIncludedInTasks;
        for (const auto& node : nodes)
        {
            if (nodesIncludedInTasks.contains(node->mId))
            {
                continue;
            }

            if (!parallelizableTasks.contains(node->mId) && chancesForParallelization <= parallelTaskCount)
            {
                RGTask basicTask = {
                    .pass      = node,
                    .asyncPass = nullptr,
                };
                tasks.push_back(basicTask);
                nodesIncludedInTasks.insert(node->mId);
                continue;
            }

            // Try to find a task to run in parallel.
            const auto parallelizableNodes = parallelizableTasks[node->mId]
                | std::views::filter([&](const Id_t otherId){ return mRenderGraph->getPassById(otherId)->flags.async; })
                | std::ranges::to<std::vector<Id_t>>();

            auto* selectedAsyncTask = parallelizableNodes.empty() ? nullptr : mRenderGraph->getPassById(parallelizableNodes[0]);

            RGTask parallelTask = {
                .pass      = node,
                .asyncPass = selectedAsyncTask,
            };
            tasks.push_back(parallelTask);
            nodesIncludedInTasks.insert(node->mId);
            if (selectedAsyncTask)
            {
                nodesIncludedInTasks.insert(parallelizableNodes[0]);
            }

            parallelTaskCount++;
        }

        /** Debug print for Tasks */
        for (const auto& [i, task] : std::views::enumerate(tasks))
        {
            std::cout << std::format("Task #{} > {}", i, task.pass->name);
            if (task.asyncPass)
            {
                std::cout << std::format(", {} [Parallel]", task.asyncPass->name);
            }
            std::cout << std::endl;
        }

        return tasks;
    }

    // =======================================
    // Render Graph Compiler Phase : Resources
    // =======================================


private:
    template <class T>
    static RGCompilerOutput createErrorOutput(const RGCompilerResult<T>& result)
    {
        RGCompilerOutput output;
        output.hasFailed = true;
        output.failReason = result.error();
        return output;
    }

    /** Transform a list of Node IDs to a list of Node Pointers. (No exists check) */
    std::vector<Pass*> toNodePtrList(const std::vector<Id_t>& nodeIds) const noexcept
    {
        return nodeIds
            | std::views::transform([this](const int32_t id){ return mRenderGraph->getPassById(id); })
            | std::ranges::to<std::vector<Pass*>>();
    }

    /** Transform a list of Node Pointers to a list of Node IDs. */
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

    static RGCompilerResult<Pass*> getRootNode(RenderGraph& renderGraph)
    {
        const auto beginPass = std::ranges::find_if(renderGraph.mVertices,[](const auto& pass) {
            return pass->flags.sentinel && pass->name == rgRootPassName;
        });

        if (beginPass == std::end(renderGraph.mVertices))
        {
            return std::unexpected(RGCompilerError::NoRootNode);
        }
        return beginPass->get();
    }

    RenderGraph* mRenderGraph {nullptr};

    const RGCompilerOptions mOptions;
};
