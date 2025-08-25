cmake_minimum_required(VERSION 3.30)
project(graphCompilerPrototype)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(external/json)

add_executable(graphCompilerPrototype main.cpp
    compiler/InputData.h
    compiler/IdSequence.h
    compiler/Graph.h
    compiler/RenderGraph.h
    compiler/GraphCompiler.h
    compiler/RenderGraphExport.h
)

target_link_libraries(graphCompilerPrototype PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(graphCompilerPrototype PRIVATE external/json/include)
