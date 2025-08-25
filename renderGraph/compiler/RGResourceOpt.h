#pragma once

#include "RGCompilerTypes.h"
#include "RGResourceOptTypes.h"
#include "../RenderGraph.h"

// =======================================
// Render Graph Resource Optimizer
// =======================================
class RenderGraphResourceOptimizer
{
public:
    explicit RenderGraphResourceOptimizer(const RenderGraph* renderGraph, const std::vector<RGTask>& tasks)
    : mRenderGraph(renderGraph)
    , mTasks(tasks)
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
                .originalNode     = res.originNode->mId,
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

private:
    std::vector<ResourceInfo> evaluateRequiredResources() const noexcept
    {
        std::vector<ResourceInfo> result;

        // Get all output resources
        for (const auto& node : mRenderGraph->mVertices)
        {
            for (auto& resource : node->dependencies | std::views::filter([](const Resource& res){ return res.access == AccessType::Write; }))
            {
                const auto it = std::ranges::find_if(mTasks, [&](const RGTask& task){
                    return task.pass->mId == node->mId
                        || (task.asyncPass && task.asyncPass->mId == node->mId);
                });

                const auto i = static_cast<int32_t>(std::distance(std::begin(mTasks), it));
                result.push_back(ResourceInfo::createFrom(node.get(), resource, i));
            }
        }

        // Find resource consumers
        for (auto& resourceInfo : result)
        {
            for (const auto& edge : mRenderGraph->mEdges)
            {
                if (   resourceInfo.originNodeId         != edge.src->mId
                    || resourceInfo.originNodeId         == edge.dst->mId
                    || resourceInfo.originResource->id   != edge.srcRes
                ) continue;

                int32_t consumerNodeId     = edge.dst->mId;
                const auto consumerResource = std::ranges::find_if(edge.dst->dependencies, [&](const Resource& res){
                    return res.id == edge.dstRes;
                });
                const int32_t consumerResourceId = consumerResource->id;

                const auto it = std::ranges::find_if(mTasks, [&](const RGTask& task){
                    return task.pass->mId == consumerNodeId
                        || (task.asyncPass && task.asyncPass->mId == consumerNodeId);
                });

                const auto consumerNodeIdx = static_cast<int32_t>(std::distance(std::begin(mTasks), it));
                Pass* consumerNode = it->pass->mId == consumerNodeId ? it->pass : it->asyncPass;

                ConsumerInfo consumerInfo = {
                    .nodeId       = consumerNodeId,
                    .nodeIdx      = consumerNodeIdx,
                    .nodeName     = consumerNode->name,
                    .resourceId   = consumerResourceId,
                    .resourceName = edge.dstResName,
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

    const RenderGraph*         mRenderGraph;
    const std::vector<RGTask>& mTasks;
};
