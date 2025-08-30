#pragma once

#include "IdSequence.h"
#include "RenderGraphCore.h"

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

    inline PassPtr computeExample()
    {
        using enum ResourceType;
        using enum AccessType;
        auto pass = std::make_unique<Pass>();

        pass->mId = IdSequence::next();
        pass->name = "AsyncCompute Pass";
        pass->flags = {
            .compute = true,
            .async = true,
        };
        pass->dependencies = {
            Resource { IdSequence::next(), "scene", External, None },
            Resource { IdSequence::next(), "someImage", Image, Write },
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
            Resource { IdSequence::next(), "motionVectors", Image, Write },
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

    inline PassPtr graphicsAntiAliasingPass()
    {
        using enum ResourceType;
        using enum AccessType;
        auto pass = std::make_unique<Pass>();

        pass->mId = IdSequence::next();
        pass->name = "Anti-Aliasing Pass";
        pass->flags = {
            .raster = true,
        };
        pass->dependencies = {
            Resource { IdSequence::next(), "motionVectors", Image, Read },
            Resource { IdSequence::next(), "aaInput", Image, Read },
            Resource { IdSequence::next(), "aaOutput", Image, Write },
        };

        return pass;
    }

    inline PassPtr sentinelPresentPass()
    {
        using enum ResourceType;
        using enum AccessType;
        auto pass = std::make_unique<Pass>();

        pass->mId = IdSequence::next();
        pass->name = rgPresentPass;
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
        pass->name = rgRootPass;
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
