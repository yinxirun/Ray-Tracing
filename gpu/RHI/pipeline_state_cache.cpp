#include "pipeline_state_cache.h"
#include "gpu/vk/rhi.h"
#include <memory>

namespace PipelineStateCache
{
    VertexDeclaration *GetOrCreateVertexDeclaration(const VertexDeclarationElementList &Elements)
    {
        std::shared_ptr<VertexDeclaration> NewDeclaration = RHI::Get().CreateVertexDeclaration(Elements);
        return NewDeclaration.get();
    }
}