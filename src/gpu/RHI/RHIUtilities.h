#pragma once
#include "RHIDefinitions.h"
#include "RHIAccess.h"

/** Get the best default resource state for the given texture creation flags */
extern Access RHIGetDefaultResourceState(ETextureCreateFlags InUsage, bool bInHasInitialData);