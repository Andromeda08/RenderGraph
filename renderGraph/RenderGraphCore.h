#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Graph.h"

#ifdef rg_JSON_EXPORT
    #include <nlohmann/json.hpp>
#endif


// =======================================
// Render Graph : Macros
// =======================================
#define rg_TRACE

// The following macros are for debugging / validation purposes.
#ifdef rg_TRACE
    // Declare trace parameters in a function declaration
    #define rg_DECL_TRACE_PARAMS , const char* file = __builtin_FILE(), int line = __builtin_LINE(), const char* callerFn = __builtin_FUNCTION()

    // Declare trace parameters for a function implementation
    #define rg_TRACE_PARAMS , const char* file, int line, const char* callerFn

    // Assert that a function was called from one that is allowed to call it.
    #define rg_CALL_GUARD(file, line, callerFn, whitelist, message) \
        if (!whitelist.contains(callerFn)) \
        { \
            const std::filesystem::path path {file}; \
            std::println("!!! Function {} was called from {} (file: {}, line: {}) : {}", \
                __builtin_FUNCTION(), callerFn, path.filename().c_str(), line, message); \
            exit(EXIT_FAILURE); \
        }
#else
    #define rg_DECL_TRACE_PARAMS
    #define rg_TRACE_PARAMS
    #define rg_CALL_GUARD(file, line, callerFn, whitelist, message)
#endif

// =======================================
// Render Graph : Forward Decl., Constants
// =======================================
struct Pass;
struct PassFlags;

struct Resource;
struct ResourceFlags;

enum class AccessType;
enum class ResourceType;

class RenderGraph;
struct RGTask;
struct Edge;

using Id_t    = int32_t;
using PassPtr = std::unique_ptr<Pass>;

constexpr Id_t              rgInvalidId      = -1;
constexpr std::string_view  rgRootPass       = "Root";
constexpr std::string_view  rgPresentPass    = "Present";
constexpr std::string_view  rgUnknownEnumStr = "unknown";

// =======================================
// Render Graph : Enum Types
// =======================================
enum class AccessType
{
    None,
    Read,
    Write,
};
constexpr std::string toString(const AccessType accessType) noexcept
{
    using enum AccessType;
    switch (accessType)
    {
        case Read  : return "read";
        case Write : return "write";
        case None  : return "none";
    }
    return std::string(rgUnknownEnumStr);
}

#ifdef rg_JSON_EXPORT
NLOHMANN_JSON_SERIALIZE_ENUM(AccessType, {
    {AccessType::Read,  "read" },
    {AccessType::Write, "write"},
    {AccessType::None,  "none" },
})
#endif

enum class ResourceType
{
    Unknown,
    Image,
    Buffer,
    External,
};
constexpr std::string toString(const ResourceType resourceType) noexcept
{
    using enum ResourceType;
    switch (resourceType)
    {
        case Unknown  : return "unknown";
        case Image    : return "image";
        case Buffer   : return "buffer";
        case External : return "external";
    }
    return std::string(rgUnknownEnumStr);
}

#ifdef rg_JSON_EXPORT
NLOHMANN_JSON_SERIALIZE_ENUM(ResourceType, {
    {ResourceType::Unknown,  "unknown" },
    {ResourceType::Image,    "image"   },
    {ResourceType::Buffer,   "buffer"  },
    {ResourceType::External, "external"},
})
#endif

// =======================================
// Render Graph : Resource, Pass
// =======================================
struct ResourceFlags
{
    bool dontOptimize = false;  // Don't consider this resource during Resource Optimization phase.
};

/**
 * (1) "Resource" can now be simple as this, as the exact specifications are only required for
 * pass-specific resource allocation, as Images are now memory aliased.
 * (2) This can now be used to represent both pass-specific resources and render graph resources.
 * (GPU resource vs GPU memory)
 * (3) AccessType is ignored for resources of type "External". This is because the RenderGraph is not responsible
 * for managing an external resources state, neither are external resources required to be GPU resources.
 */
struct Resource
{
    Id_t            id;
    std::string     name;
    ResourceType    type;
    AccessType      access;
    ResourceFlags   flags;
};

struct PassFlags
{
    bool raster     = false;    // Any pass that's not Async or Compute
    bool compute    = false;    // Compute Pass
    bool async      = false;    // Async Pass
    bool neverCull  = false;    // Don't allow the culling of the pass
    bool sentinel   = false;    // Begin / Present "Pass"
};

struct Pass final : Vertex
{
    ~Pass() override = default;

    Id_t getId() const { return mId; }

    Resource* getResource(const std::string& resourceName);

    Resource* getResource(Id_t resourceId);

    std::string             name;
    PassFlags               flags;
    std::vector<Resource>   dependencies;
};

// =======================================
// Render Graph : Data Types
// =======================================
struct RGTask
{
    Pass* pass;
    Pass* asyncPass;
};

struct Edge
{
    Id_t        id;
    Pass*       src;
    Pass*       dst;
    Resource*   pSrcRes = nullptr;
    Resource*   pDstRes = nullptr;
};
