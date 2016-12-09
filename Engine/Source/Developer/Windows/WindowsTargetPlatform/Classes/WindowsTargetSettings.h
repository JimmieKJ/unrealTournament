// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowsTargetSettings.h: Declares the UWindowsTargetSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WindowsTargetSettings.generated.h"

UENUM()
enum class EMinimumSupportedOS : uint8
{
	MSOS_Vista = 0 UMETA(DisplayName = "Windows Vista"),
};

UENUM()
enum class ECompilerVersion : uint8
{
	Default = 0,
	VisualStudio2013 = 1 UMETA(DisplayName = "Visual Studio 2013"),
	VisualStudio2015 = 2 UMETA(DisplayName = "Visual Studio 2015"),
	VisualStudio2017 = 3 UMETA(DisplayName = "Visual Studio '15'"),
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

	/** The compiler version to use for this project. May be different to the chosen IDE. */
	UPROPERTY(EditAnywhere, config, Category="Toolchain", Meta=(DisplayName="Compiler Version"))
	ECompilerVersion Compiler;

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
	EMinimumSupportedOS MinimumOSVersion;

	/** The audio device name to use if not the default windows audio device. Leave blank to use default audio device. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString AudioDevice;

};
