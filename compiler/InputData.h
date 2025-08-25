#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "Graph.h"
#include "IdSequence.h"

// =======================================
// Constants
// =======================================
constexpr int32_t          rgInvalidId    = -1;
constexpr std::string_view rgRootPassName = "Root";

// =======================================
// Resources
// =======================================

// Can AccessType be inferred in an easy way?
enum class AccessType
{
    Read,
    Write,
    None,
};
NLOHMANN_JSON_SERIALIZE_ENUM(AccessType, {
    {AccessType::Read,  "read" },
    {AccessType::Write, "write"},
    {AccessType::None,  "none" },
})

enum class ResourceType
{
    Unknown,
    Image,
    Buffer,
    External,
};

struct ResourceFlags
{
    bool dontOptimize = false;  // Don't consider this resource during Resource Optimization phase.
};

/**
 * (1) "Resource" can now be simple as this, as the exact specifications are only required for
 * pass-specific resource allocation, as Images are now memory aliased.
 * (2) This can now be used to represent both pass-specific resources and render graph resources.
 * (GPU resource vs GPU memory)
 * (3) AccessType is ignored for resources of type "External". This is because the RenderGraph is not responsible
 * for managing an external resources state, neither are external resources required to be GPU resources.
 */
struct Resource
{
    int32_t         id = rgInvalidId;
    std::string     name;
    ResourceType    type;
    AccessType      access;
    ResourceFlags   flags = {};
};

// =======================================
// Passes
// =======================================

struct PassFlags
{
    bool raster     = false;    // Any pass that's not Async or Compute
    bool compute    = false;    // Compute Pass
    bool async      = false;    // Async Pass
    bool neverCull  = false;    // Don't allow the culling of the pass
    bool sentinel   = false;    // Begin / Present "Pass"
};

struct Pass final : Vertex
{
    ~Pass() override = default;

    std::string             name;
    PassFlags               flags;
    std::vector<Resource>   dependencies;
};

using PassPtr = std::unique_ptr<Pass>;

namespace Passes
{
    inline PassPtr computeAmbientOcclusion()
    {
        using enum ResourceType;
        using enum AccessType;
        auto pass = std::make_unique<Pass>();

        pass->mId = IdSequence::next();
        pass->name = "Ambient Occlusion Pass";
        pass->flags = {
                .raster = true,
                .compute = true,
                .async = true,
        };
        pass->dependencies = {
            Resource { IdSequence::next(), "positionImage", Image, Read },
            Resource { IdSequence::next(), "normalImage", Image, Read },
            Resource { IdSequence::next(), "ambientOcclusionImage", Image, Write },
        };

        return pass;
    }

    inline PassPtr graphicsGBufferPass()
    {
        using enum ResourceType;
        using enum AccessType;
        auto pass = std::make_unique<Pass>();

        pass->mId = IdSequence::next();
        pass->name = "G-Buffer Pass";
        pass->flags = {
            .raster = true,
        };
        pass->dependencies = {
            Resource { IdSequence::next(), "scene", External, None },
            Resource { IdSequence::next(), "positionImage", Image, Write },
            Resource { IdSequence::next(), "normalImage", Image, Write },
            Resource { IdSequence::next(), "albedoImage", Image, Write },
        };

        return pass;
    }

    inline PassPtr graphicsLightingPass()
    {
        using enum ResourceType;
        using enum AccessType;
        auto pass = std::make_unique<Pass>();

        pass->mId = IdSequence::next();
        pass->name = "Lighting Pass";
        pass->flags = {
            .raster = true,
        };
        pass->dependencies = {
            Resource { IdSequence::next(), "positionImage", Image, Read },
            Resource { IdSequence::next(), "normalImage", Image, Read },
            Resource { IdSequence::next(), "albedoImage", Image, Read },
            Resource { IdSequence::next(), "lightingResult", Image, Write },
        };

        return pass;
    }

    inline PassPtr utilCompositionPass()
    {
        using enum ResourceType;
        using enum AccessType;
        auto pass = std::make_unique<Pass>();

        pass->mId = IdSequence::next();
        pass->name = "Composition Pass";
        pass->flags = {
            .raster = true,
        };
        pass->dependencies = {
            Resource { IdSequence::next(), "imageA", Image, Read },
            Resource { IdSequence::next(), "imageB", Image, Read },
            Resource { IdSequence::next(), "combined", Image, Write },
        };

        return pass;
    }

    inline PassPtr sentinelPresentPass()
    {
        using enum ResourceType;
        using enum AccessType;
        auto pass = std::make_unique<Pass>();

        pass->mId = IdSequence::next();
        pass->name = "Present Pass";
        pass->flags = {
            .raster = true,
            .neverCull = true,
            .sentinel = true,
        };
        pass->dependencies = {
            Resource { IdSequence::next(), "presentImage", Image, Read },
        };

        return pass;
    }

    inline PassPtr sentinelBeginPass()
    {
        using enum ResourceType;
        using enum AccessType;
        auto pass = std::make_unique<Pass>();

        pass->mId = IdSequence::next();
        pass->name = rgRootPassName;
        pass->flags = {
            .neverCull = true,
            .sentinel = true,
        };
        pass->dependencies = {
            Resource { IdSequence::next(), "scene", External, None },
        };

        return pass;
    }
}
