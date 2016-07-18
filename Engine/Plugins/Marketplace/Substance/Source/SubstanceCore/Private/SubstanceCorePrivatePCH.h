//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include <Engine.h>
#if WITH_EDITOR
#include "UnrealEd.h"
#endif

DECLARE_MEMORY_STAT_EXTERN(TEXT("Substance memory"),STAT_SubstanceMemory,STATGROUP_SceneMemory,SUBSTANCECORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Substance cache memory"),STAT_SubstanceCacheMemory,STATGROUP_SceneMemory,SUBSTANCECORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Substance engine memory"),STAT_SubstanceEngineMemory,STATGROUP_SceneMemory,SUBSTANCECORE_API);

#include "SubstanceCoreTypedefs.h"
#include "ImageWrapper.h"
#include "IImageWrapper.h"
#include "ISubstanceCore.h"
#include "SubstanceCoreCustomVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubstanceCore, Warning, All);
