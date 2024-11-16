#pragma once

#include "gpu/definitions.h"
#include "gpu/RHI/RHIDefinitions.h"

#include <iostream>

class RHIResource;

/** Dereferences the RHI resource from a shader parameter struct. */
inline RHIResource *GetShaderParameterResourceRHI(const void *Contents, uint16 MemberOffset, EUniformBufferBaseType MemberType)
{
    check(Contents);
    if (IsShaderParameterTypeIgnoredByRHI(MemberType))
    {
        return nullptr;
    }

    const uint8 *MemberPtr = (const uint8 *)Contents + MemberOffset;

    if (IsRDGResourceReferenceShaderParameterType(MemberType))
    {
        printf("Don't support RDG %s %d\n", __FILE__, __LINE__);
        exit(-1);
        /* const FRDGResource* ResourcePtr = *reinterpret_cast<const FRDGResource* const*>(MemberPtr);
        return ResourcePtr ? ResourcePtr->GetRHI() : nullptr; */
    }
    else
    {
        return *reinterpret_cast<RHIResource *const *>(MemberPtr);
    }
}