cmake_minimum_required(VERSION 3.31)
project(graphCompilerPrototype)

set(CMAKE_CXX_STANDARD 23)

add_executable(graphCompilerPrototype main.cpp
    compiler/InputData.h
    compiler/Utils.h
    compiler/Graph.h
    compiler/RenderGraph.h
    compiler/GraphCompiler.h)
