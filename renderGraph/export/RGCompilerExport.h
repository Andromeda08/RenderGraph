#pragma once

class RenderGraph;
struct RGCompilerOutput;

// =======================================
// Compiler Output Exporter
// =======================================
class RenderGraphCompilerExport
{
public:
    static void exportJSONCompilerOutput(const RGCompilerOutput& output, const RenderGraph* renderGraph);

    static void exportMermaidCompilerOutput(const RGCompilerOutput& output);
};
