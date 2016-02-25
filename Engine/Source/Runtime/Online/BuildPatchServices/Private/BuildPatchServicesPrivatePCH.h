// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchServicesPrivatePCH.h: Pre-compiled header file for the 
	BuildPatchServices module.
=============================================================================*/

#pragma once

// Still saving out old chunk data filenames
// WARNING!! Disabling this will cause older clients to be incapable of patching via chunked patches. (e.g. game-patch for Community Portal project)
//           Only remove if the latest versions pushed of clients support new filename chunk data.
#define SAVE_OLD_CHUNKDATA_FILENAMES 0

// Still saving out old data file format
// WARNING!! Disabling this will cause older clients to be incapable of patching via -nochunks patches. (e.g. self-patch for Community Portal project)
//           Only remove if absolutely sure there could be no clients in the wild that cannot fetch new filename file data.
#define SAVE_OLD_FILEDATA_FILENAMES 0

#include "BuildPatchServices.h"

/* Dependencies
*****************************************************************************/
#include "Core.h"
#include "SecureHash.h"
#include "Json.h"
#include "Http.h"
#include "DefaultValueHelper.h"
#include "IAnalyticsProvider.h"

/* Public includes
*****************************************************************************/
#include "IBuildManifest.h"
#include "IBuildPatchServicesModule.h"

/* Private includes
*****************************************************************************/
#include "BuildPatchError.h"
#include "BuildPatchProgress.h"
#include "BuildPatchHTTP.h"
#include "BuildPatchChunk.h"
#include "BuildPatchManifest.h"
#include "BuildPatchVerification.h"
#include "BuildPatchFileAttributes.h"
#include "BuildPatchChunkCache.h"
#include "BuildPatchCompactifier.h"
#include "BuildPatchDataEnumeration.h"
#include "BuildPatchFileConstructor.h"
#include "BuildPatchDownloader.h"
#include "BuildPatchInstaller.h"
#include "BuildPatchServicesModule.h"
#include "BuildPatchHash.h"
#include "BuildPatchGeneration.h"
#include "BuildPatchUtil.h"
#include "BuildPatchAnalytics.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBuildPatchServices, Log, All);
