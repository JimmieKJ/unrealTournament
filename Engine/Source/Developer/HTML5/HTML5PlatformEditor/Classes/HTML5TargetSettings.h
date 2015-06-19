// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5TargetSettings.h: Declares the UHTML5TargetSettings class.
=============================================================================*/

#pragma once

#include "HTML5TargetSettings.generated.h"



USTRUCT()
struct FHTML5LevelTransitions
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = HTML5_LevelTransitions, Meta = (DisplayName = "From Map"))
	FFilePath MapFrom;

	UPROPERTY(EditAnywhere, Category = HTML5_LevelTransitions, Meta = (DisplayName = "To Map"))
	FFilePath MapTo;
};


/**
 * Implements the settings for the HTML5 target platform.
 */
UCLASS(config=Engine, defaultconfig)
class HTML5PLATFORMEDITOR_API UHTML5TargetSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()
 	/**
 	 * Setting to control HTML5 Heap size (in Development)
 	 */
 	UPROPERTY(GlobalConfig, EditAnywhere, Category=Memory, Meta = (DisplayName = "Development Heap Size (in MB)", ClampMin="1", ClampMax="4096"))
 	int32 HeapSizeDevelopment;

	/**
 	 * Setting to control HTML5 Heap size
 	 */
 	UPROPERTY(GlobalConfig, EditAnywhere, Category=Memory, Meta = (DisplayName = "Heap Size (in MB)", ClampMin="1", ClampMax="4096"))
 	int32 HeapSizeShipping;

	/**
 	 * Port to use when deploying game from the editor
 	 */
 	UPROPERTY(GlobalConfig, EditAnywhere, Category=Memory, Meta = (DisplayName = "Port to use when deploying game from the editor", ClampMin="49152", ClampMax="65535"))
 	int32 DeployServerPort;


	/**
	* Setting to control which template to use
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Packaging, Meta = (DisplayName = "Use new template [experimental]"))
	bool UseExperimentalTemplate;

	/**
	 * Use a loading level and download maps during transitions.                                                                     
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Packaging, Meta = (DisplayName = "Download maps on the fly [experimental]"))
	bool UseAsyncLevelLoading;

	/**
	 * Generate Delta Pak files for these level transitions.
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Packaging, Meta = (DisplayName = "Level transitions for delta paks [experimental,depends on download maps]"))
	TArray<FHTML5LevelTransitions> LevelTransitions;

};
