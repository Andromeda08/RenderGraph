#pragma once

#include <algorithm>
#include <fstream>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "InputData.h"

using Id_t = int32_t;

// =======================================
// Render Graph : Data Types
// =======================================
struct RGTask
{
    Pass* pass       = nullptr;
    Pass* asyncPass  = nullptr;
};

struct Edge
{
    Id_t        id;
    Pass*       src;
    std::string srcRes;
    Pass*       dst;
    std::string dstRes;
};

class RenderGraph
{
public:
    /** Add a Pass to the RenderGraph */
    Pass* addPass(std::unique_ptr<Pass>&& vtx)
    {
        mVertices.push_back(std::move(vtx));
        return mVertices.back().get();
    }

    bool deletePass(Id_t passId);

    /** Insert an edge between pass resources.
     * @return Success value
     */
    bool insertEdge(Pass* src, const std::string& srcRes, Pass* dst, const std::string& dstRes)
    {
        if (src->mId == dst->mId) return false;

        if (const auto findSrcRes = std::ranges::find_if(src->dependencies, [&](const Resource& res){ return res.name == srcRes; });
            findSrcRes == std::end(src->dependencies))
        {
            return false;
        }

        if (const auto findDstRes = std::ranges::find_if(dst->dependencies, [&](const Resource& res){ return res.name == dstRes; });
            findDstRes == std::end(dst->dependencies))
        {
            return false;
        }

        src->mOutgoingEdges.push_back(dst);
        dst->mIncomingEdges.push_back(src);

        mEdges.emplace_back(IdSequence::next(), src, srcRes, dst, dstRes);

        return true;
    }

    /** Delete an edge between pass resources.
     * @return Success value
     */
    bool deleteEdge(Pass* src, const std::string& srcRes, Pass* dst, const std::string& dstRes)
    {
        if (src->mId == dst->mId) return false;

        const auto edge = std::ranges::find_if(mEdges, [&](const Edge& e) {
            return e.src    == src    && e.dst    == dst
                && e.srcRes == srcRes && e.dstRes == dstRes;
        });
        if (edge == std::end(mEdges)) return false;

        const auto b_find = std::ranges::find_if(src->mOutgoingEdges, [&](auto& v){ return v->mId == dst->mId; });
        if (b_find == std::end(src->mOutgoingEdges)) return false;

        const auto a_find = std::ranges::find_if(dst->mIncomingEdges, [&](auto& v){ return v->mId == src->mId; });
        if (a_find == std::end(dst->mIncomingEdges)) return false;

        mEdges.erase(edge);

        src->mOutgoingEdges.erase(b_find);
        dst->mIncomingEdges.erase(a_find);

        return true;
    }

    /** Delete an edge between pass resources.
     * @return Success value
     */
    bool deleteEdge(const Edge& edge)
    {
        return deleteEdge(edge.src, edge.srcRes, edge.dst, edge.dstRes);
    }

    bool containsEdge(const Pass* src, const Pass* dst) noexcept
    {
        return std::ranges::find_if(mEdges, [src, dst](const Edge& edge) {
            return edge.src == src && edge.dst == dst;
        }) != std::end(mEdges);
    }

    bool containsEdge(const Pass* src, const std::string& srcRes, const Pass* dst, const std::string& dstRes) noexcept
    {
        return std::ranges::find_if(mEdges, [src, dst, &srcRes, &dstRes](const Edge& edge) {
            return edge.src    == src    && edge.dst    == dst
                && edge.srcRes == srcRes && edge.dstRes == dstRes;
        }) != std::end(mEdges);
    }

    bool containsAnyEdge(const Pass* a, const Pass* b) noexcept
    {
        return containsEdge(a, b) || containsEdge(b, a);
    }

    /** Transform a list of Node IDs to a list of Node Pointers. (No exists check) */
    std::vector<Pass*> toNodePtrList(const std::vector<Id_t>& nodeIds) const noexcept
    {
        return nodeIds
            | std::views::transform([this](const int32_t id){ return getPassById(id); })
            | std::ranges::to<std::vector<Pass*>>();
    }

    // =======================================
    // Getters
    // =======================================
    Pass* getPassById(const Id_t id) const noexcept
    {
        const auto pass = std::ranges::find_if(mVertices, [&id](const auto& p){ return id == p->mId; });
        return pass == std::end(mVertices)
             ? nullptr
             : pass->get();
    }

    const std::vector<PassPtr>& getVertices() const { return mVertices; }
    const std::vector<Edge>&    getEdges()    const { return mEdges;    }

private:
    friend class RenderGraphCompiler;
    friend class RenderGraphResourceOptimizer;
    friend class RenderGraphExport;
    friend class RenderGraphCompilerExport;

    static RenderGraph createCopy(const RenderGraph& renderGraph)
    {
        RenderGraph copyGraph;

        for (const auto& node : renderGraph.mVertices)
        {
            auto* pass = copyGraph.addPass(std::make_unique<Pass>());
            pass->dependencies = node->dependencies;
            pass->name = node->name;
            pass->mId = node->mId;
            pass->flags = node->flags;
        }

        for (const auto& edge : renderGraph.mEdges)
        {
            auto* newSrc = copyGraph.getPassById(edge.src->mId);
            auto* newDst = copyGraph.getPassById(edge.dst->mId);
            copyGraph.insertEdge(newSrc, edge.srcRes, newDst, edge.dstRes);
        }

        return copyGraph;
    }

    std::vector<PassPtr> mVertices;
    std::vector<Edge>    mEdges;
};

inline RenderGraph* createExampleGraph()
{
    auto* graph = new RenderGraph();

    Pass* beginPass     = graph->addPass(Passes::sentinelBeginPass());
    Pass* gBufferPass   = graph->addPass(Passes::graphicsGBufferPass());
    Pass* lightingPass  = graph->addPass(Passes::graphicsLightingPass());
    Pass* aoPass        = graph->addPass(Passes::computeAmbientOcclusion());
    Pass* compPass      = graph->addPass(Passes::utilCompositionPass());
    Pass* presentPass   = graph->addPass(Passes::sentinelPresentPass());

    std::vector<bool> edgeInserts;

    edgeInserts.append_range(std::vector {
        graph->insertEdge(beginPass, "scene", gBufferPass, "scene"),

        graph->insertEdge(gBufferPass, "positionImage", lightingPass, "positionImage"),
        graph->insertEdge(gBufferPass, "normalImage", lightingPass, "normalImage"),
        graph->insertEdge(gBufferPass, "albedoImage", lightingPass, "albedoImage"),

        graph->insertEdge(gBufferPass, "positionImage", aoPass, "positionImage"),
        graph->insertEdge(gBufferPass, "normalImage", aoPass, "normalImage"),

        graph->insertEdge(lightingPass, "lightingResult", compPass, "imageA"),
        graph->insertEdge(aoPass, "ambientOcclusionImage", compPass, "imageB"),

        graph->insertEdge(compPass, "combined", presentPass, "presentImage"),
    });

    if (!std::ranges::all_of(edgeInserts, [](const bool& val){ return val == true;}))
    {
        throw std::runtime_error("Some edge insertions failed");
    }

    return graph;
}
