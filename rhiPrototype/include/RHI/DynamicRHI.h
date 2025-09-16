#pragma once

#include "RHI.h"

nbl_INTERFACE(DynamicRHI,
    virtual void waitIdle() = 0;

    virtual RHIFrame beginFrame() = 0;
    virtual void     submitFrame(const RHIFrame& frame) = 0;

    virtual RHISwapchain*       getSwapchain() = 0;

    virtual RHICommandQueue*    getGeneralQueue() = 0;
    virtual RHICommandQueue*    getAsyncQueue()   = 0;

    virtual UPtr<RHIBuffer>     createBuffer    (const RHIBufferCreateInfo&     createInfo) = 0;
    virtual UPtr<RHIDescriptor> createDescriptor(const RHIDescriptorCreateInfo& createInfo) = 0;
    virtual UPtr<RHIPipeline>   createPipeline  (const RHIPipelineCreateInfo&   createInfo) = 0;
    virtual UPtr<RHIRenderPass> createRenderPass(const RHIRenderPassCreateInfo& createInfo) = 0;
    virtual UPtr<RHITexture>    createTexture   (const RHITextureCreateInfo&    createInfo) = 0;
);

static UPtr<DynamicRHI> createDynamicRHI(const RHICreateInfo& createInfo);
