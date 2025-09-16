# ============================ #
# Rendering Hardware Interface #
# ============================ #
add_library(RHI
    include/RHI/RHI.h
    source/RHI.cpp
    include/RHI/DynamicRHI.h
    include/RHI/Definitions.h
    include/Private/Flags.h
    include/Private/Utility.h
    include/RHI/String.h
)
target_include_directories(RHI PRIVATE
    ./include
)

# ============================ #
# VulkanRHI                    #
# ============================ #
find_package(Vulkan REQUIRED)
add_library(VulkanRHI
    source/VulkanRHI/vk.cpp
    include/VulkanRHI/Interface.h
    include/VulkanRHI/Descriptor.h
)
target_link_libraries(VulkanRHI PUBLIC ${Vulkan_LIBRARIES})
target_include_directories(VulkanRHI PRIVATE
    ./include
    ${Vulkan_INCLUDE_DIRS}
)

# ============================ #
# RHI : Tests                  #
# ============================ #
add_executable(rhi_test
    test/rhi_test.cpp
)
target_link_libraries(rhi_test PRIVATE RHI)
target_include_directories(rhi_test PRIVATE ./include)
