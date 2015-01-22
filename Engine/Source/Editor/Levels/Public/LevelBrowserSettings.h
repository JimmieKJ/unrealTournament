// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LevelBrowserSettings.h: Declares the ULevelBrowserSettings class.
=============================================================================*/

#pragma once

#include "LevelBrowserSettings.generated.h"


/**
 * Implements settings for the level browser.
 */
UCLASS(config=EditorUserSettings)
class LEVELS_API ULevelBrowserSettings
	:	public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** True if the actor count is displayed in the slate level browser */
	UPROPERTY(config)
	uint32 bDisplayActorCount:1;

	/** True if the Lightmass Size is displayed in the slate level browser */
	UPROPERTY(config)
	uint32 bDisplayLightmassSize:1;

	/** True if the File Size is displayed in the slate level browser */
	UPROPERTY(config)
	uint32 bDisplayFileSize:1;

	/** True if Level Paths are displayed in the slate level browser */
	UPROPERTY(config)
	uint32 bDisplayPaths:1;

	/** True if the Editor Offset is displayed in the slate level browser */
	UPROPERTY(config)
	uint32 bDisplayEditorOffset:1;
};
