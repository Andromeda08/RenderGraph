#pragma once

#include <cstdint>
#include <ranges>
#include <stack>
#include <vector>

#include "RGCompilerTypes.h"
#include "../RenderGraphCore.h"

enum class RGBarrierType
{
    RaW, WaR, RaR, WaW, None,
};

struct RGBarrier
{
    int32_t       taskIdx;
    Id_t          nodeId;
    RGBarrierType type;
};

struct RGBarrierBatch
{
    int32_t                 taskIdx;
    std::vector<RGBarrier>  barriers;
};

struct RGBarrierGenParams
{
    std::vector<RGTask>             taskOrder;
    std::vector<RGResourceTemplate> resources;
};

struct RGBarrierGen
{
    static std::vector<RGBarrierBatch> generateBarriers(const RGBarrierGenParams& params)
    {
        // Index of RGResourceTemplate in <params.resources> mapped to a stack tracking the last access type.
        std::map<int32_t, std::stack<AccessType>> resourceAccessTracker;

        std::vector<RGBarrierBatch> barrierBatches;
        for (const auto& task : params.taskOrder)
        {
            const auto taskResources = params.resources
                | std::views::filter([&task](const RGResourceTemplate& res) { return isUsedByTask(res, task); });

        }
    }
};
