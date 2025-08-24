#include <iostream>
#include <stdexcept>

#include "compiler/GraphCompiler.h"
#include "compiler/InputData.h"
#include "compiler/RenderGraph.h"

int main()
{
    RenderGraph* renderGraph;

    try {
        renderGraph = createExampleGraph();
    } catch (const std::runtime_error& err)
    {
        std::cerr << err.what() << std::endl;
        return 1;
    }

    constexpr RGCompilerOptions compilerOptions = {
        .allowParallelization = true,
    };
    const RenderGraphCompiler compiler(renderGraph, compilerOptions);
    auto result = compiler.compile();

    return 0;
}