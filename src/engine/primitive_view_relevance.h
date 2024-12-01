#pragma once
#include "definitions.h"
class PrimitiveViewRelevance
{
public:
    /** The primitive's static elements are rendered for the view. */
    uint32 bStaticRelevance : 1;
    /** The primitive's dynamic elements are rendered for the view. */
    uint32 bDynamicRelevance : 1;
    /** The primitive is drawn. */
    uint32 bDrawRelevance : 1;

    /** The primitive should render to the base pass / normal depth / velocity rendering. */
    uint32 bRenderInMainPass : 1;
};