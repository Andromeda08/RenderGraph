#include <iostream>
#include <stdexcept>

#include "renderGraph/InputData.h"
#include "renderGraph/RenderGraph.h"
#include "renderGraph/compiler/RGCompiler.h"

int main()
{
    std::unique_ptr<RenderGraph> renderGraph;

    try {
        renderGraph = createExampleGraph2();
    } catch (const std::runtime_error& err)
    {
        std::cerr << err.what() << std::endl;
        return 1;
    }

    constexpr RGCompilerOptions compilerOptions = {
        .allowParallelization = true,
    };
    const RenderGraphCompiler compiler(renderGraph.get(), compilerOptions);
    try {
        auto result = compiler.compile();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}