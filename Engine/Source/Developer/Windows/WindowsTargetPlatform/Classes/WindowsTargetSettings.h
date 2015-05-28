// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowsTargetSettings.h: Declares the UWindowsTargetSettings class.
=============================================================================*/

#pragma once

#include "WindowsTargetSettings.generated.h"

UENUM()
enum class EMinimumSupportedOS : uint8
{
	MSOS_Vista = 0 UMETA(DisplayName = "Windows Vista"),
	MSOS_XP = 1 UMETA(DisplayName = "Windows XP"),
};

/**
 * Implements the settings for the Windows target platform.
 */
UCLASS(config=Engine, defaultconfig)
class WINDOWSTARGETPLATFORM_API UWindowsTargetSettings
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

	/**
	 * Determine the minimum supported 
	 */
	UPROPERTY(EditAnywhere, config, Category="OS Info", Meta=(DisplayName = "Minimum OS Version"))
	TEnumAsByte<EMinimumSupportedOS> MinimumOSVersion;
};
