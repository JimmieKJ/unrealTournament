// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/* Globals
 *****************************************************************************/

SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlate, Log, All);
SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlateStyles, Log, All);

DECLARE_STATS_GROUP(TEXT("Slate Memory"), STATGROUP_SlateMemory, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Slate"), STATGROUP_Slate, STATCAT_Advanced);
DECLARE_STATS_GROUP_VERBOSE(TEXT("SlateVerbose"), STATGROUP_SlateVerbose, STATCAT_Advanced);


// Compile all the RichText and MultiLine editable text?
#define WITH_FANCY_TEXT 1

/* Forward declarations
*****************************************************************************/
class FActiveTimerHandle;
enum class EActiveTimerReturnType : uint8;
