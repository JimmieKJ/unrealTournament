// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CookerSettings.h: Declares the UCookerSettings class.
=============================================================================*/

#pragma once

#include "CookerSettings.generated.h"

/**
 * Various cooker settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Cooker"))
class UNREALED_API UCookerSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(GlobalConfig, EditAnywhere, Category = Cooker, meta = (DisplayName = "Enable cooking via network in the background of the editor, launch on uses this setting, requires device to have network access to editor"))
	bool bEnableCookOnTheSide;


	/** Quality of 0 means fastest, 4 means best quality */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Textures, meta = (DisplayName = "PVRTC Compression Quality (0-4, 0 is fastest)"))
	int32 DefaultPVRTCQuality;

	/** Quality of 0 means fastest, 3 means best quality */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Textures, meta = (DisplayName = "ASTC Compression Quality vs Speed (0-4, 0 is fastest)"))
	int32 DefaultASTCQualityBySpeed;

	/** Quality of 0 means smallest (12x12 block size), 4 means best (4x4 block size) */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Textures, meta = (DisplayName = "ASTC Compression Quality vs Size (0-4, 0 is smallest)"))
	int32 DefaultASTCQualityBySize;
};
