#pragma once

#include <algorithm>
#include <cstdint>
#include <format>
#include <ranges>
#include <set>
#include <string>
#include <vector>
#include "../InputData.h"

#ifdef rg_JSON_EXPORT
    #include <nlohmann/json.hpp>
#endif

constexpr bool isOptimizableResource(const ResourceType resourceType)
{
    return resourceType == ResourceType::Image;
}

struct ConsumerInfo
{
    Id_t        nodeId     = rgInvalidId;
    int32_t     nodeIdx    = -1;
    std::string nodeName;
    Id_t        resourceId = rgInvalidId;
    std::string resourceName;
    AccessType  access     = AccessType::None;
    Pass*       node       = nullptr;
};

struct ResourceInfo
{
    Id_t                      originNodeId     = rgInvalidId;
    int32_t                   originNodeIdx    = -1;
    Pass*                     originNode       = nullptr;
    Id_t                      originResourceId = rgInvalidId;
    Resource*                 originResource   = nullptr;
    AccessType                originAccess     = AccessType::None;
    ResourceType              type             = ResourceType::Unknown;
    bool                      optimizable      = true;
    std::vector<ConsumerInfo> consumers        = {};

    static ResourceInfo createFrom(Pass* pass, Resource& resource, const int32_t execOrder)
    {
        return {
            .originNodeId       = pass->mId,
            .originNodeIdx      = execOrder,
            .originNode         = pass,
            .originResourceId   = resource.id,
            .originResource     = &resource,
            .originAccess       = resource.access,
            .type               = resource.type,
            .optimizable        = isOptimizableResource(resource.type),
            .consumers          = {},
        };
    }
};

struct UsagePoint
{
    int32_t     point      = {};
    int32_t     userResId  = rgInvalidId;
    std::string usedAs;
    int32_t     userNodeId = rgInvalidId;
    std::string usedBy;
    AccessType  access     = AccessType::None;

    UsagePoint() = default;

    explicit UsagePoint(const ConsumerInfo& consumerInfo)
    {
        point      = consumerInfo.nodeIdx;
        userResId  = consumerInfo.resourceId;
        usedAs     = consumerInfo.resourceName;
        userNodeId = consumerInfo.nodeId;
        usedBy     = consumerInfo.nodeName;
        access     = consumerInfo.access;
    }

    explicit UsagePoint(const ResourceInfo& resourceInfo)
    {
        point      = resourceInfo.originNodeIdx;
        userResId  = resourceInfo.originResourceId;
        usedAs     = resourceInfo.originResource->name;
        userNodeId = resourceInfo.originNodeId;
        usedBy     = resourceInfo.originNode->name;
        access     = resourceInfo.originResource->access;
    }
};
#ifdef rg_JSON_EXPORT
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UsagePoint, point, userResId, usedAs, userNodeId, usedBy, access);
#endif

inline bool operator<(const UsagePoint& lhs, const UsagePoint& rhs)
{
    return lhs.point < rhs.point;
}

inline bool operator==(const UsagePoint& lhs, const UsagePoint& rhs)
{
    return lhs.point == rhs.point;
}

struct Range
{
    int32_t start;
    int32_t end;

    Range() = default;

    explicit Range(const std::set<UsagePoint>& points)
    {
        const auto min = std::min_element(std::begin(points), std::end(points));
        const auto max = std::max_element(std::begin(points), std::end(points));

        start = min->point;
        end = max->point;

        validate();
    }

    Range(const int32_t a, const int32_t b): start(a), end(b)
    {
        validate();
    }

    bool overlaps(const Range& other) const
    {
        return std::max(start, other.start) <= std::min(end, other.end);
    }

private:
    void validate() const
    {
        if (start > end)
        {
            throw std::runtime_error(std::format("Range starting point {} is greater than the end point {}", start, end));
        }
    }
};

struct RGOptResource
{
    int32_t              id;
    std::set<UsagePoint> usagePoints;
    Resource             originalResource;
    Id_t                 originalNode;
    ResourceType         type;

    Range getUsageRange() const
    {
        return Range(usagePoints);
    }

    std::optional<UsagePoint> getUsagePoint(const int32_t value) const
    {
        const auto find = std::ranges::find_if(usagePoints, [&](const UsagePoint& up){ return up.point == value; });
        return (find == std::end(usagePoints))
            ? std::nullopt
            : std::make_optional(*find);
    }

    bool insertUsagePoints(const std::set<UsagePoint>& points)
    {
        // Validation for occupied usage points
        std::vector<UsagePoint> intersection;
        std::set_intersection(std::begin(usagePoints), std::end(usagePoints), std::begin(points), std::end(points), std::back_inserter(intersection));

        if (!intersection.empty())
        {
            return false;
        }

        for (const auto& point: points)
        {
            usagePoints.insert(point);
        }

        return true;
    }
};

struct RGResOptOutput
{
    std::vector<RGOptResource>  generatedResources;

    // Input
    std::vector<Resource>       originalResources;

    // Statistics
    int32_t nonOptimizables = 0;
    int32_t reduction       = 0;
    int32_t preCount        = 0;
    int32_t postCount       = 0;
    Range   timelineRange   = { 0, 0 };
};
