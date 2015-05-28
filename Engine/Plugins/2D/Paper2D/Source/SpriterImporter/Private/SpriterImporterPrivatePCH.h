// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealEd.h"
#include "Paper2DModule.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpriterImporter, Verbose, All);

#define SPRITER_IMPORT_ERROR(FormatString, ...) \
	if (!bSilent) { UE_LOG(LogSpriterImporter, Warning, FormatString, __VA_ARGS__); }
#define SPRITER_IMPORT_WARNING(FormatString, ...) \
	if (!bSilent) { UE_LOG(LogSpriterImporter, Warning, FormatString, __VA_ARGS__); }

