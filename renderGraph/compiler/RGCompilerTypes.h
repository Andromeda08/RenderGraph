#pragma once

#include <expected>
#include <map>
#include <optional>
#include <vector>

// =======================================
// Type Aliases & Forward Declarations
// =======================================
struct RGTask;
struct RGResOptOutput;

using Node_t = Pass*;

// =======================================
// Error & Result Type
// =======================================
enum class RGCompilerError
{
    None,
    NoRootNode,
    CyclicDependency,
    NoNodeByGivenId,
};

template <class T>
using RGCompilerResult = std::expected<T, RGCompilerError>;

// =======================================
// Compiler Data Types
// =======================================
struct RGCompilerOptions
{
    bool allowParallelization = false;
};

struct RGResourceLink
{
    Id_t        srcPass;
    Id_t        dstPass;
    Id_t        srcResource;
    Id_t        dstResource;
    AccessType  access;
};

struct RGResourceTemplate
{
    Id_t                        id;
    ResourceType                type;
    std::vector<RGResourceLink> links;
};

// =======================================
#include "RGResourceOptTypes.h"
// =======================================

struct RGCompilerPhaseOutputs
{
    std::vector<Id_t>                   cullNodes;
    std::vector<Id_t>                   serialExecutionOrder;
    std::map<Id_t, std::vector<Id_t>>   parallelizableNodes;
    std::vector<RGTask>                 taskOrder;
    RGResOptOutput                      resourceOptimizer;
};

struct RGCompilerOutput
{
    std::vector<RGResourceTemplate>       resourceTemplates;
    bool                                  hasFailed     = false;
    RGCompilerError                       failReason    = RGCompilerError::None;
    std::optional<RGCompilerPhaseOutputs> phaseOutputs  = std::nullopt;
    RGCompilerOptions                     options       = {};
};