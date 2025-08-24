#pragma once
#include "InputData.h"

struct Edge
{
    int32_t     id;
    Pass*       src;
    std::string srcRes;
    Pass*       dst;
    std::string dstRes;
};

class RenderGraph
{
public:
    ~RenderGraph()
    {
        for (const auto* vertex : mVertices)
        {
            delete vertex;
        }
    }

    Pass* addPass(Pass* vtx)
    {
        mVertices.push_back(vtx);
        return vtx;
    }

    bool insertEdge(Pass* src, const std::string& srcRes, Pass* dst, const std::string& dstRes)
    {
        if (src->mId == dst->mId) return false;

        if (const auto findSrcRes = std::ranges::find_if(src->dependencies, [&](const Resource& res){ return res.name == srcRes; });
            findSrcRes == std::end(src->dependencies))
        {
            std::cerr << std::format("Pass {} has no Resource {}", src->name, srcRes) << std::endl;
            return false;
        }

        if (const auto findDstRes = std::ranges::find_if(dst->dependencies, [&](const Resource& res){ return res.name == dstRes; });
            findDstRes == std::end(dst->dependencies))
        {
            std::cerr << std::format("Pass {} has no Resource {}", dst->name, dstRes) << std::endl;
            return false;
        }

        src->mOutgoingEdges.push_back(dst);
        dst->mIncomingEdges.push_back(src);

        mEdges.emplace_back(IdSequence::next(), src, srcRes, dst, dstRes);

        return true;
    }

    bool deleteEdge(const Edge& edge)
    {
        return deleteEdge(edge.src, edge.srcRes, edge.dst, edge.dstRes);
    }

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

    Pass* getPassById(const int32_t id) const noexcept
    {
        const auto pass = std::ranges::find_if(mVertices, [&id](const Pass* p){ return id == p->mId; });
        return pass == std::end(mVertices)
             ? nullptr
             : *pass;

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

    std::vector<Pass*> mVertices;
    std::vector<Edge>  mEdges;
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
