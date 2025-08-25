#include "RenderGraphExport.h"

#include <chrono>
#include <format>
#include <fstream>
#include <ranges>
#include <set>
#include <string>
#include <vector>
#include "../RenderGraph.h"

void RenderGraphExport::exportMermaid(const RenderGraph* renderGraph)
{
    std::vector<std::string> output = {
        "flowchart TD",
        "classDef resImage color:#4c4f69,fill:#cba6f7,stroke:#8839ef,stroke-width:1px;",
        "classDef resOther color:#4c4f69,fill:#f38ba8,stroke:#d20f39,stroke-width:1px;",
        "classDef pass color:#4c4f69,fill:#b4befe,stroke:#7287fd,stroke-width:1px;",
    };

    for (const auto& node : renderGraph->mVertices)
    {
        output.emplace_back(std::format("{}[{}]:::pass", node->mId, node->name));
        for (const auto& edge : renderGraph->mEdges)
        {
            if (node->mId == edge.src->mId)
            {
                const auto res = std::ranges::find_if(node->dependencies, [&](const Resource& resource){ return resource.name == edge.srcRes; });
                const auto type = res->type;
                output.emplace_back(std::format("{}({}):::{}", edge.srcRes, edge.srcRes, type == ResourceType::Image ? "resImage" : "resOther"));
            }
        }
    }

    std::set<std::pair<Id_t, std::string>> edges;
    for (const auto& start : renderGraph->mVertices)
    {
        for (const auto& edge : renderGraph->mEdges)
        {
            if (start->mId == edge.src->mId)
            {
                const auto edge1 = std::format("{} --> {}", start->mId, edge.srcRes);
                if (const auto it = std::ranges::find_if(output, [&edge1](const std::string& str){ return str == edge1; });
                    it == std::end(output))
                {
                    output.push_back(edge1);
                }
                const auto edge2 = std::format("{} --> {}", edge.srcRes, edge.dst->mId);
                if (const auto it = std::ranges::find_if(output, [&edge2](const std::string& str){ return str == edge2; });
                    it == std::end(output))
                {
                    output.push_back(edge2);
                }
            }
        }
    }

    auto sysTime = std::chrono::system_clock::now();
    std::ofstream file(std::format("export/renderGraph_{:%Y-%m-%d_%H-%M}.mermaid", sysTime));
    for (const auto& s : output)
    {
        file << s << '\n';
    }
}

void RenderGraphExport::exportGraphvizDOT(const RenderGraph* renderGraph)
{
    std::vector<std::string> output = {"digraph {"};
    for (const auto& start : renderGraph->mVertices)
    {
        for (const auto& endVtx : start->mOutgoingEdges)
        {
            auto* end = dynamic_cast<Pass*>(endVtx);
            output.emplace_back(std::format(R"("{}" -> "{}")", start->name, end->name));
        }
    }
    output.emplace_back("}");

    std::ofstream file("renderGraph.dot");
    for (const auto& s : output)
    {
        file << s << '\n';
    }
}
