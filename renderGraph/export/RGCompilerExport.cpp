#include "RGCompilerExport.h"

#include <chrono>
#include <fstream>
#include <ranges>
#include <string>
#include <vector>

#include "../RenderGraph.h"
#include "../compiler/RGCompilerTypes.h"

#ifdef rg_JSON_EXPORT
    #include <nlohmann/json.hpp>
#endif

void RenderGraphCompilerExport::exportJSONCompilerOutput(const RGCompilerOutput& output, const RenderGraph* renderGraph)
{
#ifdef rg_JSON_EXPORT
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
#else
    return;
#endif
}

void RenderGraphCompilerExport::exportMermaidCompilerOutput(const RGCompilerOutput& output)
{
    if (!output.phaseOutputs.has_value())
    {
        return;
    }

    std::vector<std::string> out = {
        "---",
        "displayMode: compact",
        "---",
        "gantt",
        "\tdateFormat X",
        "\taxisFormat %s",
        "\tsection Passes",
    };

    for (const auto& [i, task] : std::views::enumerate(output.phaseOutputs->taskOrder))
    {
        out.push_back(std::format("\t\t{} : {}, {}", task.pass->name, i, i + 1));
    }

    out.emplace_back("\tsection Async");
    for (const auto& [i, task] : std::views::enumerate(output.phaseOutputs->taskOrder))
    {
        if (task.asyncPass)
        {
            out.push_back(std::format("\t\t{} :crit, {}, {}", task.asyncPass->name, i, i + 1));
        }
    }

    for (const auto& [i, resource] : std::views::enumerate(output.phaseOutputs->resourceOptimizer.generatedResources))
    {
        out.emplace_back(std::format("\tsection Resource #{}", i));
        auto usagePoints = std::ranges::to<std::vector<UsagePoint>>(resource.usagePoints);

        for (int32_t j = 1; j < usagePoints.size(); j++)
        {
            if (usagePoints[j - 1].access == AccessType::Write)
            {
                usagePoints[j].usedAs = usagePoints[j - 1].usedAs;
            }
        }

        std::map<std::string, Range> usageRanges;
        for (const auto& usagePoint : usagePoints)
        {
            if (!usageRanges.contains(usagePoint.usedAs))
            {
                Range range = {};
                range.start = usagePoint.point;
                range.end   = usagePoint.point;
                usageRanges[usagePoint.usedAs] = range;
            }
            else
            {
                usageRanges[usagePoint.usedAs].end = usagePoint.point;
            }
        }

        for (const auto& [usedAs, range] : usageRanges)
        {
            out.emplace_back(std::format("\t\t{} : {}, {}", usedAs, range.start, range.end + 1));
        }
    }

    auto sysTime = std::chrono::system_clock::now();
    std::ofstream file(std::format("export/renderGraphCompiled_{:%Y-%m-%d_%H-%M}.mermaid", sysTime));
    for (const auto& s : out)
    {
        file << s << '\n';
    }
}
