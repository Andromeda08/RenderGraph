find_package(Vulkan REQUIRED)

add_library(RHI inc/rhi/rhi.h src/rhi.cpp
    inc/rhi/DynamicRHI.h
    inc/rhi/def.h
    inc/rhi/macro.h
)

target_include_directories(RHI PRIVATE inc/rhi ${Vulkan_INCLUDE_DIRS})
target_link_libraries(RHI PRIVATE ${Vulkan_LIBRARIES})

add_executable(rhi_test test/rhi_test.cpp)
target_link_libraries(rhi_test PRIVATE RHI)
target_include_directories(rhi_test PRIVATE inc)