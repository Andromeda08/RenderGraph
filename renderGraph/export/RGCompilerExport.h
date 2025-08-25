#pragma once

#include <fstream>
#include <ranges>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../RenderGraph.h"
#include "../compiler/RGCompilerTypes.h"

// =======================================
// Compiler Output Exporter
// =======================================
class RenderGraphCompilerExport
{
public:
    static void exportJSONCompilerOutput(const RGCompilerOutput& output, const RenderGraph* renderGraph);

    static void exportMermaidCompilerOutput(const RGCompilerOutput& output);
};
