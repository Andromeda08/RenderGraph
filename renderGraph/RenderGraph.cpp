#include "RenderGraph.h"

#include <algorithm>
#include <fstream>
#include <ranges>
#include "InputData.h"

Pass* RenderGraph::addPass(std::unique_ptr<Pass>&& vtx)
{
    mVertices.push_back(std::move(vtx));
    return mVertices.back().get();
}

bool RenderGraph::deletePass(const Id_t passId)
{
    auto const* pass = getPassById(passId);
    if (!pass)
    {
        return false;
    }

    std::erase_if(mEdges, [pass](const Edge& edge) {
        return edge.src->getId() == pass->getId()
            || edge.dst->getId() == pass->getId();
    });
    std::erase_if(mVertices, [pass](const std::unique_ptr<Pass>& p) {
        return p->getId() == pass->getId();
    });

    return true;
}

bool RenderGraph::insertEdge(Pass* src, const std::string& srcRes, Pass* dst, const std::string& dstRes)
{
    if (src->mId == dst->mId) { return false; }

    auto* pSrcRes = src->getResource(srcRes);
    if (!pSrcRes) { return false; }

    auto* pDstRes = dst->getResource(dstRes);
    if (!pDstRes) { return false; }

    src->mOutgoingEdges.push_back(dst);
    dst->mIncomingEdges.push_back(src);

    mEdges.emplace_back(IdSequence::next(), src,dst, pSrcRes, pDstRes);

    return true;
}

bool RenderGraph::deleteEdge(Pass* src, const std::string& srcRes, Pass* dst, const std::string& dstRes)
{
    if (src->mId == dst->mId) return false;

    const auto* pSrcRes = src->getResource(srcRes);
    if (!pSrcRes) { return false; }

    const auto* pDstRes = src->getResource(dstRes);
    if (!pDstRes) { return false; }

    const auto edge = std::ranges::find_if(mEdges, [&](const Edge& e) {
        return e.src->getId() == src->getId() && e.dst->getId() == dst->getId()
               && e.pSrcRes->id  == pSrcRes->id  && e.pDstRes->id  == pDstRes->id;
    });
    if (edge == std::end(mEdges)) { return false; }

    const auto b_find = std::ranges::find_if(src->mOutgoingEdges, [&](auto& v){ return v->mId == dst->mId; });
    if (b_find == std::end(src->mOutgoingEdges)) { return false; }

    const auto a_find = std::ranges::find_if(dst->mIncomingEdges, [&](auto& v){ return v->mId == src->mId; });
    if (a_find == std::end(dst->mIncomingEdges)) { return false; }

    mEdges.erase(edge);

    src->mOutgoingEdges.erase(b_find);
    dst->mIncomingEdges.erase(a_find);

    return true;
}

bool RenderGraph::deleteEdge(const Edge& edge)
{
    return deleteEdge(edge.src, edge.pSrcRes->name, edge.dst, edge.pDstRes->name);
}

bool RenderGraph::containsEdge(const Pass* src, const Pass* dst) noexcept
{
    return std::ranges::find_if(mEdges, [src, dst](const Edge& edge) {
        return edge.src->getId() == src->getId() && edge.dst->getId() == dst->getId();
    }) != std::end(mEdges);
}

bool RenderGraph::containsAnyEdge(const Pass* a, const Pass* b) noexcept
{
    return containsEdge(a, b) || containsEdge(b, a);
}

std::vector<Pass*> RenderGraph::toNodePtrList(const std::vector<Id_t>& nodeIds) const noexcept
{
    return nodeIds
        | std::views::transform([this](const int32_t id){ return getPassById(id); })
        | std::ranges::to<std::vector<Pass*>>();
}

Pass* RenderGraph::getPassById(const Id_t id) const noexcept
{
    const auto pass = std::ranges::find_if(mVertices, [&id](const auto& p){ return id == p->mId; });
    return pass == std::end(mVertices)
        ? nullptr
        : pass->get();
}

RenderGraph RenderGraph::createCopy(const RenderGraph& renderGraph)
{
    RenderGraph copyGraph;

    for (const auto& node : renderGraph.mVertices)
    {
        auto* pass = copyGraph.addPass(std::make_unique<Pass>());
        pass->dependencies = node->dependencies;
        pass->name         = node->name;
        pass->mId          = node->mId;
        pass->flags        = node->flags;
    }

    for (const auto& edge : renderGraph.mEdges)
    {
        auto* newSrc = copyGraph.getPassById(edge.src->mId);
        auto* newDst = copyGraph.getPassById(edge.dst->mId);
        copyGraph.insertEdge(newSrc, edge.pSrcRes->name, newDst, edge.pDstRes->name);
    }

    return copyGraph;
}

std::unique_ptr<RenderGraph> createExampleGraph()
{
    auto graph = std::make_unique<RenderGraph>();

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

std::unique_ptr<RenderGraph> createExampleGraph2()
{
    auto graph = std::make_unique<RenderGraph>();

    Pass* beginPass     = graph->addPass(Passes::sentinelBeginPass());
    Pass* someCompute   = graph->addPass(Passes::computeExample());
    Pass* gBufferPass   = graph->addPass(Passes::graphicsGBufferPass());
    Pass* lightingPass  = graph->addPass(Passes::graphicsLightingPass());
    Pass* aoPass        = graph->addPass(Passes::computeAmbientOcclusion());
    Pass* compPass      = graph->addPass(Passes::utilCompositionPass());
    Pass* aaPass        = graph->addPass(Passes::graphicsAntiAliasingPass());
    Pass* compPass2     = graph->addPass(Passes::utilCompositionPass());
    Pass* presentPass   = graph->addPass(Passes::sentinelPresentPass());

    std::vector<bool> edgeInserts;

    edgeInserts.append_range(std::vector {
        graph->insertEdge(beginPass, "scene", gBufferPass, "scene"),

        graph->insertEdge(beginPass, "scene", someCompute, "scene"),

        graph->insertEdge(gBufferPass, "positionImage", lightingPass, "positionImage"),
        graph->insertEdge(gBufferPass, "normalImage", lightingPass, "normalImage"),
        graph->insertEdge(gBufferPass, "albedoImage", lightingPass, "albedoImage"),

        graph->insertEdge(gBufferPass, "positionImage", aoPass, "positionImage"),
        graph->insertEdge(gBufferPass, "normalImage", aoPass, "normalImage"),

        graph->insertEdge(lightingPass, "lightingResult", compPass, "imageA"),
        graph->insertEdge(aoPass, "ambientOcclusionImage", compPass, "imageB"),

        graph->insertEdge(compPass, "combined", aaPass, "aaInput"),
        graph->insertEdge(gBufferPass, "motionVectors", aaPass, "motionVectors"),

        graph->insertEdge(aaPass, "aaOutput", compPass2, "imageA"),
        graph->insertEdge(someCompute, "someImage", compPass2, "imageB"),

        graph->insertEdge(compPass2, "combined", presentPass, "presentImage"),
    });

    if (!std::ranges::all_of(edgeInserts, [](const bool& val){ return val == true;}))
    {
        throw std::runtime_error("Some edge insertions failed");
    }

    return graph;
}
