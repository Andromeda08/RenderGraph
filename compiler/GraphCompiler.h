#pragma once

#include <expected>
#include <optional>
#include <ranges>
#include <sstream>
#include <nlohmann/json.hpp>

#include "RenderGraph.h"
#include "RenderGraphExport.h"

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
// Using Defines
// =======================================

template <class T>
using RGCompilerResult = std::expected<T, RGCompilerError>;

using Node_t = Pass*;

// =======================================
// Render Graph Resource Optimizer : Data
// =======================================
constexpr bool isOptimizableResource(const ResourceType resourceType)
{
    return resourceType == ResourceType::Image;
}

struct ConsumerInfo
{
    Id_t        nodeId     = rgInvalidId;
    int32_t     nodeIdx    = -1;
    std::string nodeName;
    Id_t        resourceId = rgInvalidId;
    std::string resourceName;
    AccessType  access     = AccessType::None;
    Pass*       node       = nullptr;
};

struct ResourceInfo
{
    Id_t                      originNodeId     = rgInvalidId;
    int32_t                   originNodeIdx    = -1;
    Pass*                     originNode       = nullptr;
    Id_t                      originResourceId = rgInvalidId;
    Resource*                 originResource   = nullptr;
    AccessType                originAccess     = AccessType::None;
    ResourceType              type             = ResourceType::Unknown;
    bool                      optimizable      = true;
    std::vector<ConsumerInfo> consumers        = {};

    static ResourceInfo createFrom(Pass* pass, Resource& resource, const int32_t execOrder)
    {
        return {
            .originNodeId       = pass->mId,
            .originNodeIdx      = execOrder,
            .originNode         = pass,
            .originResourceId   = resource.id,
            .originResource     = &resource,
            .originAccess       = resource.access,
            .type               = resource.type,
            .optimizable        = isOptimizableResource(resource.type),
            .consumers          = {},
        };
    }
};

struct UsagePoint
{
    int32_t     point      = {};
    int32_t     userResId  = rgInvalidId;
    std::string usedAs;
    int32_t     userNodeId = rgInvalidId;
    std::string usedBy;
    AccessType  access     = AccessType::None;

    UsagePoint() = default;

    explicit UsagePoint(const ConsumerInfo& consumerInfo)
    {
        point      = consumerInfo.nodeIdx;
        userResId  = consumerInfo.resourceId;
        userNodeId = consumerInfo.nodeId;
        access     = consumerInfo.access;
    }

    explicit UsagePoint(const ResourceInfo& resourceInfo)
    {
        point      = resourceInfo.originNodeIdx;
        userResId  = resourceInfo.originResourceId;
        usedAs     = resourceInfo.originResource->name;
        userNodeId = resourceInfo.originNodeId;
        usedBy     = resourceInfo.originNode->name;
        access     = resourceInfo.originResource->access;
    }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UsagePoint, point, userResId, usedAs, userNodeId, usedBy, access);

inline bool operator<(const UsagePoint& lhs, const UsagePoint& rhs)
{
    return lhs.point < rhs.point;
}

inline bool operator==(const UsagePoint& lhs, const UsagePoint& rhs)
{
    return lhs.point == rhs.point;
}

struct Range
{
    int32_t start;
    int32_t end;

    explicit Range(const std::set<UsagePoint>& points)
    {
        const auto min = std::min_element(std::begin(points), std::end(points));
        const auto max = std::max_element(std::begin(points), std::end(points));

        start = min->point;
        end = max->point;

        validate();
    }

    Range(const int32_t a, const int32_t b): start(a), end(b)
    {
        validate();
    }

    bool overlaps(const Range& other) const
    {
        return std::max(start, other.start) <= std::min(end, other.end);
    }

private:
    void validate() const
    {
        if (start > end)
        {
            throw std::runtime_error(std::format("Range starting point {} is greater than the end point {}", start, end));
        }
    }
};

struct RGOptResource
{
    int32_t              id;
    std::set<UsagePoint> usagePoints;
    Resource             originalResource;
    ResourceType         type;

    Range getUsageRange() const
    {
        return Range(usagePoints);
    }

    std::optional<UsagePoint> getUsagePoint(const int32_t value) const
    {
        const auto find = std::ranges::find_if(usagePoints, [&](const UsagePoint& up){ return up.point == value; });
        return (find == std::end(usagePoints))
            ? std::nullopt
            : std::make_optional(*find);
    }

    bool insertUsagePoints(const std::set<UsagePoint>& points)
    {
        // Validation for occupied usage points
        std::vector<UsagePoint> intersection;
        std::set_intersection(std::begin(usagePoints), std::end(usagePoints), std::begin(points), std::end(points), std::back_inserter(intersection));

        if (!intersection.empty())
        {
            return false;
        }

        for (const auto& point: points)
        {
            usagePoints.insert(point);
        }

        return true;
    }
};

// =======================================
// Render Graph Resource Optimizer
// =======================================

struct RGResOptOutput
{
    std::vector<RGOptResource>  generatedResources;

    // Input
    std::vector<Resource>       originalResources;

    // Statistics
    int32_t nonOptimizables = 0;
    int32_t reduction       = 0;
    int32_t preCount        = 0;
    int32_t postCount       = 0;
    Range   timelineRange   = { 0, 0 };
};

class RenderGraphResourceOptimizer
{
public:
    explicit RenderGraphResourceOptimizer(const RenderGraph* renderGraph)
    : mRenderGraph(renderGraph)
    {
    }

    RGCompilerResult<RGResOptOutput> run() const
    {
        const auto R = evaluateRequiredResources();
        std::vector<RGOptResource> generatedResources;

        int32_t nonOptmizeableCount = 0;
        for (const auto& res : R)
        {
            RGOptResource resource = {
                .id               = IdSequence::next(),
                .usagePoints      = {},
                .originalResource = *res.originResource,
                .type             = res.type
            };

            auto& usagePoints = resource.usagePoints;
            usagePoints = getUsagePointsForResourceInfo(res);

            Range incomingRange(usagePoints);
            if (!res.optimizable || res.originResource->flags.dontOptimize) {
                generatedResources.push_back(resource);
                nonOptmizeableCount++;
                continue;
            }

            // Case: No generated resources yet
            if (generatedResources.empty()) {
                generatedResources.push_back(resource);
                continue;
            }

            // Case: There are generated resources, try inserting into an existing one
            bool wasInserted = false;
            for (auto& timeline : generatedResources) {
                if (const Range currentRange = timeline.getUsageRange();
                    !currentRange.overlaps(incomingRange))
                {
                    wasInserted = timeline.insertUsagePoints(usagePoints);
                    if (wasInserted) {
                        break;
                    }
                }
            }

            // Case: Failed to Insert
            if (!wasInserted) {
                generatedResources.push_back(resource);
            }
        }

        RGResOptOutput output = {
            .generatedResources = generatedResources,
            .originalResources  = R
                | std::views::transform([](const auto& resInfo){ return *resInfo.originResource; })
                | std::ranges::to<std::vector<Resource>>(),
            .nonOptimizables    = nonOptmizeableCount,
            .reduction          = static_cast<int32_t>(R.size() - generatedResources.size()),
            .preCount           = static_cast<int32_t>(R.size()),
            .postCount          = static_cast<int32_t>(generatedResources.size()),
            .timelineRange      = { 0, static_cast<int32_t>(mRenderGraph->mVertices.size()) },
        };

        return output;
    }

    [[deprecated]] static void exportResult(RGResOptOutput& result, const std::vector<Pass*>& execOrder)
    {
        std::vector<std::string> csv;
        std::stringstream sstr;
        sstr << "Optimized Resources,\n"
             << std::format("Reduction: {},", result.reduction)
             << std::format("Non-optimizable: {},", result.nonOptimizables);
        csv.push_back(sstr.str());
        sstr.str(std::string());

        sstr << ",";
        for (int32_t i = 0; i < result.timelineRange.end; i++)
        {
            // sstr << std::format("Node #{},", i);
            sstr << std::format("{},", execOrder[i]->name);
        }
        csv.push_back(sstr.str());
        sstr.str(std::string());

        for (int32_t i = 0; i < result.generatedResources.size(); i++)
        {
            auto& resource = result.generatedResources[i];
            auto range = resource.getUsageRange();

            sstr << std::format("Resource #{},", i);
            for (int32_t j = 0; j < result.timelineRange.end; j++)
            {
                if (auto usagePoint = resource.getUsagePoint(j); usagePoint.has_value())
                {
                    auto point = usagePoint.value();
                    sstr << ((range.start == range.end) ? std::format("[{}]", point.usedAs)
                        : (j == range.start) ? std::format("[{}", point.usedAs)
                        : (j == range.end) ? std::format("{}]", point.usedAs)
                        : point.usedAs);
                }
                sstr << ",";
            }
            csv.push_back(sstr.str());
            sstr.str(std::string());
        }

        std::fstream fs("resourceOptimizerResult.csv");
        fs.open("resourceOptimizerResult.csv", std::ios_base::out);
        std::ostream_iterator<std::string> os_it(fs, "\n");
        std::ranges::copy(csv, os_it);
        fs.close();
    }

private:
    std::vector<ResourceInfo> evaluateRequiredResources() const noexcept
    {
        std::vector<ResourceInfo> result;

        // Get all output resources
        for (const auto& [i, node] : std::views::enumerate(mRenderGraph->mVertices))
        {
            for (auto& resource : node->dependencies | std::views::filter([](const Resource& res){ return res.access == AccessType::Write; }))
            {
                result.push_back(ResourceInfo::createFrom(node.get(), resource, static_cast<int32_t>(i)));
            }
        }

        // Find resource consumers
        for (auto& resourceInfo : result)
        {
            for (const auto& edge : mRenderGraph->mEdges)
            {
                if (   resourceInfo.originNodeId         != edge.src->mId
                    || resourceInfo.originNodeId         == edge.dst->mId
                    || resourceInfo.originResource->name != edge.srcRes     // TODO: Prefer ID
                ) continue;

                int32_t consumerNodeId     = edge.dst->mId;
                const auto consumerResource = std::ranges::find_if(edge.dst->dependencies, [&](const Resource& res){
                    return res.name == edge.dstRes;
                });
                const int32_t consumerResourceId = consumerResource->id;

                const auto it = std::ranges::find_if(mRenderGraph->mVertices, [&](const auto& pass){
                    return pass->mId == consumerNodeId;
                });

                const auto consumerNodeIdx = static_cast<int32_t>(std::distance(std::begin(mRenderGraph->mVertices), it));
                Pass* consumerNode = mRenderGraph->mVertices[consumerNodeIdx].get();

                ConsumerInfo consumerInfo = {
                    .nodeId       = consumerNodeId,
                    .nodeIdx      = consumerNodeIdx,
                    .nodeName     = consumerNode->name,
                    .resourceId   = consumerResourceId,
                    .resourceName = consumerResource->name,
                    .access       = consumerResource->access,
                    .node         = consumerNode,
                };

                resourceInfo.consumers.push_back(consumerInfo);
            }
        }

        return result;
    }

    static std::set<UsagePoint> getUsagePointsForResourceInfo(const ResourceInfo& resourceInfo)
    {
        std::set<UsagePoint> usagePoints;

        const UsagePoint producerUsagePoint(resourceInfo);
        usagePoints.insert(producerUsagePoint);

        for (auto& consumer : resourceInfo.consumers) {
            UsagePoint consumerPoint(consumer);
            usagePoints.insert(consumerPoint);
        }

        return usagePoints;
    }

    const RenderGraph* mRenderGraph;
};

// =======================================
// Data Types
// =======================================

struct RGCompilerOptions
{
    bool allowParallelization = false;
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

struct RGCompilerPhaseOutputs
{
    std::vector<Id_t>                   cullNodes;
    std::vector<Id_t>                   serialExecutionOrder;
    std::map<Id_t, std::vector<Id_t>>   parallelizableNodes;
    std::vector<RGTask>                 taskOrder;
    RGResOptOutput                      resourceOptimizer;
};

struct RGCompilerOutput
{
    bool                                  hasFailed     = false;
    RGCompilerError                       failReason    = RGCompilerError::None;
    std::optional<RGCompilerPhaseOutputs> phaseOutputs  = std::nullopt;
    RGCompilerOptions                     options       = {};
};

// =======================================
// Compiler Output Exporter
// =======================================
class RenderGraphCompilerExport
{
public:
    static void exportCompilerOutput(const RGCompilerOutput& output, const RenderGraph* renderGraph)
    {
        if (!output.phaseOutputs.has_value())
        {
            return;
        }

        const auto results = output.phaseOutputs.value();

        using namespace nlohmann;
        json graphExport = {
            { "compilerOptions", {
                { "allowParallelization", output.options.allowParallelization },
            }},
            { "inputGraph", {
                { "nodes", json::array() },
                { "edges", json::array() },
            }},
            { "serialExecutionOrder", json::array() },
            { "parallelizableNodes", json::array() },
            {"generatedTasks", json::array() },
            { "resourceOptimizerResult", {
                { "timelineLength", results.resourceOptimizer.timelineRange.end },
                { "preCount", results.resourceOptimizer.preCount },
                { "postCount", results.resourceOptimizer.postCount },
                { "reduction", results.resourceOptimizer.reduction },
                { "resources", json::array() },
            }}
        };

        // graphExport["inputGraph"]["nodes"]
        for (const auto& [i, node] : std::views::enumerate(renderGraph->mVertices))
        {
            graphExport["inputGraph"]["nodes"].push_back({
                { "id", node->mId },
                {"name", node->name },
                { "dependencies", json::array() },
            });
            for (const auto& resource : node->dependencies)
            {
                graphExport["inputGraph"]["nodes"][i]["dependencies"].push_back({
                    { "id", resource.id },
                    { "name", resource.name },
                    { "type", resource.type },
                    { "access", resource.access },
                });
            }
        }

        // graphExport["inputGraph"]["edges"]
        for (const auto& edge : renderGraph->mEdges)
        {
            graphExport["inputGraph"]["edges"].push_back({
                { "id", edge.id },
                { "srcNodeId", edge.src->mId },
                { "srcRes", edge.srcRes },
                { "dstNodeId", edge.dst->mId },
                { "dstRes", edge.dstRes },
            });
        }

        // graphExport["serialExecutionOrder"]
        for (const auto& node : renderGraph->toNodePtrList(results.serialExecutionOrder))
        {
            graphExport["serialExecutionOrder"].push_back({
                { "id", node->mId },
                { "name", node->name },
            });
        }

        // graphExport["parallelizableNodes"]
        for (const auto& [nodeId, list] : results.parallelizableNodes)
        {
            graphExport["parallelizableNodes"].push_back({
                renderGraph->getPassById(nodeId)->name, list | std::views::transform([&](const Id_t id){ return renderGraph->getPassById(id)->name; }) | std::ranges::to<std::vector<std::string>>(),
            });
        }

        // graphExport["generatedTasks"]
        for (const auto& task : results.taskOrder)
        {
            graphExport["generatedTasks"].push_back({
                { "pass", task.pass->name },
                { "async", std::format("{}", task.asyncPass ? task.asyncPass->name : "null") },
            });
        }

        // graphExport["resourceOptimizerResult"]["resources"]
        for (const auto& [i, optRes] : std::views::enumerate(results.resourceOptimizer.generatedResources))
        {
            graphExport["resourceOptimizerResult"]["resources"].push_back({
                { "id", optRes.id },
                { "type", optRes.type },
                { "usagePoints", json::array() },
            });

            for (const auto& usage : optRes.usagePoints)
            {
                graphExport["resourceOptimizerResult"]["resources"][i]["usagePoints"].push_back(usage);
            }
        }

        std::ofstream file("graphExport.json");
        file << std::setw(4) << graphExport << std::endl;
        file.close();
    }
};

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
        auto resourceOptimizerResult = optimizeResources(cullNodesResult.value());
        rg_CHECK_COMPILER_STEP_RESULT(resourceOptimizerResult);

        // =======================================
        // Export Visualization & Debug Data
        // =======================================
        RGCompilerOutput output = {
            .hasFailed    = false,
            .failReason   = RGCompilerError::None,
            .phaseOutputs = RGCompilerPhaseOutputs {
                .cullNodes              = cullNodesResult.value(),
                .serialExecutionOrder   = serialExecutionOrderResult.value(),
                .parallelizableNodes    = parallelizableTasksResult.value(),
                .taskOrder              = finalTaskOrderResult.value(),
                .resourceOptimizer      = resourceOptimizerResult.value(),
            },
            .options = mOptions,
        };

        RenderGraphExport::exportMermaid(mRenderGraph);
        RenderGraphCompilerExport::exportCompilerOutput(output, mRenderGraph);

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
            const auto nodePtrs = mRenderGraph->toNodePtrList(nodeIds) | std::ranges::to<std::vector<Vertex*>>();
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

        // RenderGraphExport::exportMermaid(mRenderGraph);
        // RenderGraphExport::exportGraphvizDOT(&shadowGraph);

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

        /** Debug print for Tasks
        for (const auto& [i, task] : std::views::enumerate(tasks))
        {
            std::cout << std::format("Task #{} > {}", i, task.pass->name);
            if (task.asyncPass)
            {
                std::cout << std::format(", {} [Parallel]", task.asyncPass->name);
            }
            std::cout << std::endl;
        }
        */

        return tasks;
    }

    // =======================================
    // Render Graph Compiler Phase : Resources
    // =======================================

    /** Render Graph Compiler : Step 3.1
     * Run the resource optimization algorithm.
     * @return Optimizer output.
     */
    RGCompilerResult<RGResOptOutput> optimizeResources(const std::vector<Id_t>& nodeIds) const noexcept
    {
        return RenderGraphResourceOptimizer(mRenderGraph).run();
    }

    /** Render Graph Compiler : Step 3.2
     * Create resource templates from optimizer result.
     * @return List of templates for the optimized resources.
     */
    RGCompilerResult<std::vector<RGResourceTemplate>> getResourceTemplates(const RGResOptOutput& optimizerOutput)
    {
        std::vector<RGResourceTemplate> templates;

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

private:
    RenderGraph* mRenderGraph {nullptr};

    const RGCompilerOptions mOptions;
};
