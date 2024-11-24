#pragma once
#include "RHIResources.h"

class RHICommandList;
class GraphicsPipelineStateInitializer;

enum class PSOPrecacheResult
{
    Unknown,      //< No known pre cache state
    Active,       //< PSO is currently precaching
    Complete,     //< PSO has been precached successfully
    Missed,       //< PSO precache miss, needs to be compiled at draw time
    TooLate,      //< PSO precache request still compiling when needed
    NotSupported, //< PSO precache not supported (VertexFactory or MeshPassProcessor doesn't support/implement precaching)
    Untracked,    //< PSO is not tracked at all (Global shader or not coming from MeshDrawCommands)
};

namespace PipelineStateCache
{
    extern VertexDeclaration *GetOrCreateVertexDeclaration(const VertexDeclarationElementList &Elements);
}