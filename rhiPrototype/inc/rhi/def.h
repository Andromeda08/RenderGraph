#pragma once

#include <cstdint>
#include <limits>
#include <numeric>
#include <type_traits>
#include <vector>

template <class T>
using UnderlyingLimits = std::numeric_limits<std::underlying_type_t<T>>;

template <class T>
inline std::size_t hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

#include "macro.h"

// ==========================
// RHI : Shaders
// ==========================
enum class RHIShaderBits : uint16_t
{
    None                = 0,
    Vertex              = 1 << 0,
    TessellationControl = 1 << 1,
    TessellationEval    = 1 << 2,
    Geometry            = 1 << 3,
    Fragment            = 1 << 4,
    Compute             = 1 << 5,
    Task                = 1 << 6,
    Mesh                = 1 << 7,
    RayGen              = 1 << 8,
    ClosestHit          = 1 << 9,
    AnyHit              = 1 << 10,
    Miss                = 1 << 11,
    Callable            = 1 << 12,
    Any                 = UnderlyingLimits<RHIShaderBits>::max(),
};
ENUM_CLASS_FLAGS(RHIShaderBits);

// ==========================
// RHI : Descriptors
// ==========================
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

struct RHIDescriptorLayoutBinding
{
    uint32_t          binding;
    uint32_t          count;
    RHIDescriptorType descriptorType;
    RHIShaderBits     shaderStages;
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
