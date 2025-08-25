#pragma once

class RenderGraph;

class RenderGraphExport
{
public:
    static void exportMermaid(const RenderGraph* renderGraph);

    static void exportGraphvizDOT(const RenderGraph* renderGraph);
};
