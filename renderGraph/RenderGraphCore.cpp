#include "RenderGraphCore.h"

#include <ranges>

Resource* Pass::getResource(const std::string& resourceName)
{
    const auto it = std::ranges::find_if(dependencies, [&resourceName](const Resource& resource) {
        return resource.name == resourceName;
    });
    return &(*it);
}

Resource* Pass::getResource(const Id_t resourceId)
{
    const auto it = std::ranges::find_if(dependencies, [&resourceId](const Resource& resource) {
        return resource.id == resourceId;
    });
    return &(*it);
}
