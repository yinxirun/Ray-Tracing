#pragma once
#include "gpu/definitions.h"
#include "gpu/core/templates/alignment_template.h"

uint32 MemCrc32(const void *InData, int32 Length, uint32 CRC = 0);

uint32 MemCrc_DEPRECATED(const void *InData, int32 Length, uint32 CRC = 0);