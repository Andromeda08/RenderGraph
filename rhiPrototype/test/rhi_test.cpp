#include <algorithm>
#include <iostream>
#include <RHI/Definitions.h>

#include "test.h"

int32_t test_DescriptorLayout_HashEquivalency()
{
    const auto layout = RHIDescriptorLayout {
        .bindings = {
            RHIDescriptorLayoutBinding {
                .binding        = 0,
                .count          = 1,
                .descriptorType = RHIDescriptorType::CombinedImageSampler,
                .shaderStages   = RHIShaderBits::All,
            },
        },
    };

    const RHIDescriptorLayout layoutCopy = layout;

    const std::size_t h1 = std::hash<RHIDescriptorLayout>{}(layout);
    const std::size_t h2 = std::hash<RHIDescriptorLayout>{}(layoutCopy);

    const bool result = h1 == h2;

    LOG_AND_RET_TEST(result);
}

int main()
{
    std::vector<int> results;

    RUN_TEST(test_DescriptorLayout_HashEquivalency);

    return std::ranges::all_of(
        results,
        [](const int v) -> bool { return v == EXIT_SUCCESS; }
    ) ? EXIT_SUCCESS : EXIT_FAILURE;
}