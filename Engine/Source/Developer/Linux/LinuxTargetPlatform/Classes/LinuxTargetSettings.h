// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxTargetSettings.h: Declares the ULinuxTargetSettings class.
=============================================================================*/

#pragma once

#include "LinuxTargetSettings.generated.h"


/**
 * Implements the settings for the Android target platform.
 */
UCLASS(config=Engine, defaultconfig)
class LINUXTARGETPLATFORM_API ULinuxTargetSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()

	/**
	 * The collection of RHI's we want to support on this platform.
	 * This is not always the full list of RHI we can support.
	 */
	UPROPERTY(EditAnywhere, config, Category=Rendering)
	TArray<FString> TargetedRHIs;
};
