#pragma once

#include <RHI/Definitions.h>
#include <vulkan/vulkan.hpp>

#include "Interface.h"

inline vk::DescriptorSetLayoutBinding toDescriptorLayoutBinding(const RHIDescriptorLayoutBinding& binding)
{
    return vk::DescriptorSetLayoutBinding()
        .setBinding(binding.binding)
        .setDescriptorCount(binding.count)
        .setDescriptorType(toDescriptorType(binding.descriptorType))
        .setStageFlags(toShaderStageFlags(binding.shaderStages));
}