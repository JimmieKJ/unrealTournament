// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BuildPatchServices.h"

/* Dependencies
*****************************************************************************/
#include "Core.h"
#include "SecureHash.h"
#include "Json.h"
#include "Http.h"
#include "DefaultValueHelper.h"

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
#include "BuildPatchMergeManifests.h"
#include "BuildPatchFileConstructor.h"
#include "BuildPatchDownloader.h"
#include "BuildPatchInstaller.h"
#include "BuildPatchServicesModule.h"
#include "BuildPatchHash.h"
#include "BuildPatchGeneration.h"
#include "BuildPatchUtil.h"
#include "BuildPatchAnalytics.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBuildPatchServices, Log, All);
