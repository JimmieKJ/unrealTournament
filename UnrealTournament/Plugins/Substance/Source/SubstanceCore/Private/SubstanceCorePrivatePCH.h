
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include <Engine.h>
#if WITH_EDITOR
#include "UnrealEd.h"
#endif

/*This flag is here to toggle on the Substance Stat System
Note - Using stat system may cause crash: Please see Stat2.h - NUM_ELEMENTS_IN_POOL */
#define SUBSTANCE_MEMORY_STAT 0

#if SUBSTANCE_MEMORY_STAT
DECLARE_MEMORY_STAT_EXTERN(TEXT("Substance memory"),STAT_SubstanceMemory,STATGROUP_SceneMemory,SUBSTANCECORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Substance cache memory"),STAT_SubstanceCacheMemory,STATGROUP_SceneMemory,SUBSTANCECORE_API);
DECLARE_MEMORY_STAT_EXTERN(TEXT("Substance engine memory"),STAT_SubstanceEngineMemory,STATGROUP_SceneMemory,SUBSTANCECORE_API);
#endif

#include "SubstanceCoreTypedefs.h"
#include "ImageWrapper.h"
#include "IImageWrapper.h"
#include "ISubstanceCore.h"
#include "SubstanceCoreCustomVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubstanceCore, Warning, All);
