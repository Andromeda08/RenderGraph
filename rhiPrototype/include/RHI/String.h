#pragma once

/* RHI : String.h
 * This file contains all string conversion functions for RHI types and enums.
 */

#include <format>
#include <string>

#include "Definitions.h"

#define EnumToStr(Value) case Value: return #Value
#define EnumToStrDefault default: return "Unknown"

// ==================================
// Enums
// ==================================
constexpr std::string toString(const RHIDescriptorType descriptorType) noexcept
{
    using enum RHIDescriptorType;
    switch (descriptorType)
    {
        EnumToStr(Sampler);
        EnumToStr(SampledImage);
        EnumToStr(CombinedImageSampler);
        EnumToStr(StorageImage);
        EnumToStr(UniformBuffer);
        EnumToStr(StorageBuffer);
        EnumToStr(AccelerationStructure);
        EnumToStrDefault;
    }
}

// ==================================
// Enum Flags
// ==================================
constexpr std::string toString(const RHIShaderBits shaderStage) noexcept
{
    using enum RHIShaderBits;
    switch (shaderStage)
    {
        EnumToStr(None);
        EnumToStr(Vertex);
        EnumToStr(TessellationControl);
        EnumToStr(TessellationEval);
        EnumToStr(Geometry);
        EnumToStr(Fragment);
        EnumToStr(Compute);
        EnumToStr(Task);
        EnumToStr(Mesh);
        EnumToStr(RayGen);
        EnumToStr(ClosestHit);
        EnumToStr(AnyHit);
        EnumToStr(Miss);
        EnumToStr(Intersection);
        EnumToStr(Callable);
        EnumToStr(All);
        EnumToStrDefault;
    }
}

// ==================================
// Descriptors Sets
// ==================================
inline std::string toString(const RHIDescriptorLayoutBinding& binding) noexcept
{
    // TODO: Introduce toString for RHIShaderStageFlags and complete the returned string
    return std::format("DescriptorLayoutBinding[binding={}, count={}, type={}]",
        binding.binding,
        binding.count,
        toString(binding.descriptorType));
}

inline std::string toString(const RHIDescriptorLayout& descriptorLayout) noexcept
{
    // TODO: complete toString method
    return "DescriptorLayout";
}