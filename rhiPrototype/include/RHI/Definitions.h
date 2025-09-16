#pragma once

/* RHI : Definitions.h
 * This file contains all RHI types and enums and related methods.
 */

#include <cstdint>
#include <limits>
#include <numeric>
#include <type_traits>
#include <vector>

#include "Private/Flags.h"
#include "Private/Utility.h"

template <class T>
using UnderlyingLimits = std::numeric_limits<std::underlying_type_t<T>>;

// ==================================
// Enums
// ==================================
enum class RHIDescriptorType
{
    Sampler,
    SampledImage,
    CombinedImageSampler,
    StorageImage,
    UniformBuffer,
    StorageBuffer,
    AccelerationStructure,
};

// ==================================
// Enum Flags
// ==================================
enum class RHIShaderBits : uint16_t
{
    None                = 0,
    // Graphics ==================
    Vertex              = 1 <<  0,
    TessellationControl = 1 <<  1,
    TessellationEval    = 1 <<  2,
    Geometry            = 1 <<  3,
    Fragment            = 1 <<  4,
    // Compute ===================
    Compute             = 1 <<  5,
    // Mesh Shading ==============
    Task                = 1 <<  6,
    Mesh                = 1 <<  7,
    // Ray Tracing ===============
    RayGen              = 1 <<  8,
    ClosestHit          = 1 <<  9,
    AnyHit              = 1 << 10,
    Miss                = 1 << 11,
    Intersection        = 1 << 12,
    Callable            = 1 << 13,
    // Other =====================
    All                 = UnderlyingLimits<RHIShaderBits>::max(),
};
using RHIShaderStageFlags = Flags<RHIShaderBits>;

// ==================================
// Descriptors Sets
// ==================================
struct RHIDescriptorLayoutBinding
{
    uint32_t          binding;
    uint32_t          count;
    RHIDescriptorType descriptorType;
    RHIShaderStageFlags    shaderStages;
};

template <>
struct std::hash<RHIDescriptorLayoutBinding>
{
    std::size_t operator()(const RHIDescriptorLayoutBinding& b) const noexcept
    {
        std::size_t result = 0;
        hash_combine(result, b.binding);
        hash_combine(result, b.count);
        hash_combine(result, static_cast<int32_t>(b.descriptorType));
        hash_combine(result, static_cast<std::underlying_type_t<RHIShaderBits>>(b.shaderStages));
        return result;
    }
};

struct RHIDescriptorLayout
{
    std::vector<RHIDescriptorLayoutBinding> bindings;
};

template <>
struct std::hash<RHIDescriptorLayout>
{
    std::size_t operator()(const RHIDescriptorLayout& layout) const noexcept
    {
        return std::reduce(layout.bindings.begin(), layout.bindings.end(), 0, [](std::size_t sum, const auto& binding) -> std::size_t {
            return hash_combine(sum, binding);
        });
    }
};

struct RHIDescriptorSetInfo
{
    uint32_t             count;
    RHIDescriptorLayout* pLayout;
};
