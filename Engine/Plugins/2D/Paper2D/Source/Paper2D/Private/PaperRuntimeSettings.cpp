// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// UPaperRuntimeSettings

UPaperRuntimeSettings::UPaperRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnableSpriteAtlasGroups(false)
	, bEnableTerrainSplineEditing(false)
	, bResizeSpriteDataToMatchTextures(true)
{
}
