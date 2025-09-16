#pragma once

/* VulkanRHI : Interface.h
 * This file contains the methods used to convert RHI types to Vulkan API specific types.
 */

#include <vulkan/vulkan.hpp>
#include <RHI/Definitions.h>

inline vk::ShaderStageFlags toShaderStageFlags(const RHIShaderStageFlags& shaderStages)
{
    vk::ShaderStageFlags result = {};

    using r = RHIShaderBits;
    using v = vk::ShaderStageFlagBits;

    if (shaderStages & r::Vertex)               result |= v::eVertex;
    if (shaderStages & r::TessellationControl)  result |= v::eTessellationControl;
    if (shaderStages & r::TessellationEval)     result |= v::eTessellationEvaluation;
    if (shaderStages & r::Geometry)             result |= v::eGeometry;
    if (shaderStages & r::Fragment)             result |= v::eFragment;
    if (shaderStages & r::Compute)              result |= v::eCompute;
    if (shaderStages & r::Task)                 result |= v::eTaskEXT;
    if (shaderStages & r::Mesh)                 result |= v::eMeshEXT;
    if (shaderStages & r::RayGen)               result |= v::eRaygenKHR;
    if (shaderStages & r::ClosestHit)           result |= v::eClosestHitKHR;
    if (shaderStages & r::AnyHit)               result |= v::eAnyHitKHR;
    if (shaderStages & r::Miss)                 result |= v::eMissKHR;
    if (shaderStages & r::Intersection)         result |= v::eIntersectionKHR;
    if (shaderStages & r::Callable)             result |= v::eCallableKHR;

    return result;
}

inline vk::DescriptorType toDescriptorType(const RHIDescriptorType& type)
{
    using enum RHIDescriptorType;
    using enum vk::DescriptorType;
    switch (type)
    {
        case Sampler:               return eSampler;
        case SampledImage:          return eSampledImage;
        case CombinedImageSampler:  return eCombinedImageSampler;
        case StorageImage:          return eStorageImage;
        case UniformBuffer:         return eUniformBuffer;
        case StorageBuffer:         return eStorageBuffer;
        case AccelerationStructure: return eAccelerationStructureKHR;
    }
}
