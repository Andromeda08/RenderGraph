cmake_minimum_required(VERSION 3.30)
project(graphCompilerPrototype)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(external/json)

add_executable(graphCompilerPrototype main.cpp
    renderGraph/InputData.h
    renderGraph/IdSequence.h
    renderGraph/Graph.h
    renderGraph/RenderGraph.h
    renderGraph/compiler/RGCompiler.h
    renderGraph/export/RenderGraphExport.h
    renderGraph/compiler/RGCompilerTypes.h
    renderGraph/compiler/RGResourceOpt.h
    renderGraph/export/RGCompilerExport.h
    renderGraph/compiler/RGResourceOptTypes.h
    renderGraph/export/RenderGraphExport.cpp
    renderGraph/export/RGCompilerExport.cpp
    renderGraph/Graph.cpp
)

target_link_libraries(graphCompilerPrototype PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(graphCompilerPrototype PRIVATE external/json/include)
target_compile_definitions(graphCompilerPrototype PRIVATE rg_JSON_EXPORT)