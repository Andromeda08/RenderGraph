#pragma once

#include <memory>
#include <string>
#include <vector>

#include "RenderGraphCore.h"

// =======================================
// Render Graph
// =======================================
class RenderGraph
{
public:
    /** Add a Pass to the RenderGraph. */
    Pass* addPass(std::unique_ptr<Pass>&& vtx);

    /** Delete a specific Pass by id. */
    bool deletePass(Id_t passId);

    /** Insert an edge between pass resources.
     * @return Success value
     */
    bool insertEdge(Pass* src, const std::string& srcRes, Pass* dst, const std::string& dstRes);

    /** Delete an edge between pass resources.
     * @return Success value
     */
    bool deleteEdge(Pass* src, const std::string& srcRes, Pass* dst, const std::string& dstRes);

    /** Delete an edge between pass resources.
     * @return Success value
     */
    bool deleteEdge(const Edge& edge);

    /** Check whether a specific directed edge exists.
     * @return Success value
     */
    bool containsEdge(const Pass* src, const Pass* dst) noexcept;

    /** Check whether an edge exists between the two vertices in any direction.
     * @return Success value
     */
    bool containsAnyEdge(const Pass* a, const Pass* b) noexcept;

    /** Transform a list of Node IDs to a list of Node Pointers. (No exists check) */
    std::vector<Pass*> toNodePtrList(const std::vector<Id_t>& nodeIds) const noexcept;

    // =======================================
    // Getters
    // =======================================
    Pass* getPassById(Id_t id) const noexcept;

    const std::vector<PassPtr>& getVertices() const { return mVertices; }
    const std::vector<Edge>&    getEdges()    const { return mEdges;    }

private:
    friend class RenderGraphCompiler;
    friend class RenderGraphResourceOptimizer;
    friend class RenderGraphExport;
    friend class RenderGraphCompilerExport;

    /**
     * Create a 1-1 copy of the specified RenderGraph
     * (Warning: IDs are also copied, this should be used by only the Compiler!)
     */
    static RenderGraph createCopy(const RenderGraph& renderGraph);

    std::vector<PassPtr> mVertices;
    std::vector<Edge>    mEdges;
};

std::unique_ptr<RenderGraph> createExampleGraph();

std::unique_ptr<RenderGraph> createExampleGraph2();
