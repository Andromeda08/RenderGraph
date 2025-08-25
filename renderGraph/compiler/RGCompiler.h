#pragma once

#include <expected>
#include <optional>
#include <ranges>

#include "../RenderGraph.h"
#include "../export/RenderGraphExport.h"
#include "../export/RGCompilerExport.h"

#include "RGCompilerTypes.h"
#include "RGResourceOpt.h"

// =======================================
// Utility Macros
// =======================================
#define rg_CHECK_COMPILER_STEP_RESULT(compilerResult) \
    if (!compilerResult.has_value()) return createErrorOutput(compilerResult)

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
        // Preamble Phase
        const auto cullNodesResult = cullNodes();
        rg_CHECK_COMPILER_STEP_RESULT(cullNodesResult);

        // Task Scheduling Phase
        const auto serialExecutionOrderResult = getSerialExecutionOrder(cullNodesResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(serialExecutionOrderResult);

        auto parallelizableTasksResult = getParallelizableTasks(serialExecutionOrderResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(parallelizableTasksResult);

        const auto finalTaskOrderResult = getFinalTaskOrder(serialExecutionOrderResult.value(), parallelizableTasksResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(finalTaskOrderResult);

         // Resource Optimizing Phase
        const auto resourceOptimizerResult = optimizeResources(finalTaskOrderResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(resourceOptimizerResult);

        // Create Templates
        const auto resourceTemplates = getResourceTemplates(resourceOptimizerResult.value());

        // Create result
        RGCompilerOutput output = {
            .resourceTemplates  = resourceTemplates,
            .hasFailed          = false,
            .failReason         = RGCompilerError::None,
            .phaseOutputs       = RGCompilerPhaseOutputs {
                .cullNodes              = cullNodesResult.value(),
                .serialExecutionOrder   = serialExecutionOrderResult.value(),
                .parallelizableNodes    = parallelizableTasksResult.value(),
                .taskOrder              = finalTaskOrderResult.value(),
                .resourceOptimizer      = resourceOptimizerResult.value(),
            },
            .options = mOptions,
        };

        // Export Visualization & Debug Data
        RenderGraphExport::exportMermaid(mRenderGraph);
        RenderGraphCompilerExport::exportMermaidCompilerOutput(output);

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
        const auto nodePtrs = mRenderGraph->toNodePtrList(nodeIds) | std::ranges::to<std::vector<Vertex*>>();

        const auto tsortResult = TopologicalSort::execute(nodePtrs);
        if (!tsortResult.has_value())
        {
            return std::unexpected(RGCompilerError::CyclicDependency);
        }

        return tsortResult.value();
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

        // RenderGraphExport::exportMermaid(mRenderGraph);
        // RenderGraphExport::exportMermaid(&shadowGraph);

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

        const auto nodes = mRenderGraph->toNodePtrList(serialExecutionOrder);

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

        return tasks;
    }

    // =======================================
    // Render Graph Compiler Phase : Resources
    // =======================================

    /** Render Graph Compiler : Step 3.1
     * Run the resource optimization algorithm.
     * @return Optimizer output.
     */
    RGCompilerResult<RGResOptOutput> optimizeResources(const std::vector<RGTask>& tasks) const noexcept
    {
        return RenderGraphResourceOptimizer(mRenderGraph, tasks).run();
    }

    // =======================================
    // Render Graph Compiler Phase : Templates
    // =======================================

    /** Render Graph Compiler : Step 4.1
     * Create resource templates from optimizer result.
     * @return List of templates for the optimized resources.
     */
    std::vector<RGResourceTemplate> getResourceTemplates(const RGResOptOutput& optimizerOutput) const
    {
        std::vector<RGResourceTemplate> templates;

        for (const auto& genRes : optimizerOutput.generatedResources)
        {
            RGResourceTemplate resource = {
                .id     = genRes.id,
                .type   = genRes.type,
                .links  = {},
            };

            const auto* originNode = mRenderGraph->getPassById(genRes.originalNode);
            for (const auto& consumer : genRes.usagePoints)
            {
                RGResourceLink link = {
                    .srcPass     = originNode->mId,
                    .dstPass     = consumer.userNodeId,
                    .srcResource = genRes.originalResource.id,
                    .dstResource = consumer.userResId,
                    .access      = consumer.access,
                };
                resource.links.push_back(link);
            }

            templates.push_back(resource);
        }

        return templates;
    }

private:
    template <class T>
    static RGCompilerOutput createErrorOutput(const RGCompilerResult<T>& result)
    {
        RGCompilerOutput output;
        output.hasFailed = true;
        output.failReason = result.error();
        return output;
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
        return static_cast<int32_t>(std::distance(std::begin(nodeIds), find));
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

private:
    RenderGraph* mRenderGraph {nullptr};

    const RGCompilerOptions mOptions;
};
