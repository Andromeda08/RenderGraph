#pragma once

#include <memory>

template <class T>
using UPtr = std::unique_ptr<T>;

#define nbl_INTERFACE(T, Interface) \
    class T { public: \
        virtual ~T() = default; \
        Interface \
    }

// Rendering Hardware Interface base class
class DynamicRHI;
struct RHICreateInfo;

// Frame Information
struct RHIFrame;

// RHI Owned Resources
class RHICommandQueue;
class RHICommandList;
class RHISwapchain;

// General GPU Resources
class RHIBuffer;
struct RHIBufferCreateInfo;

class RHIDescriptor;
struct RHIDescriptorCreateInfo;

class RHIPipeline;
struct RHIPipelineCreateInfo;

class RHIRenderPass;
struct RHIRenderPassCreateInfo;

class RHITexture;
struct RHITextureCreateInfo;

// External Library Integration
class RHIWindow;
