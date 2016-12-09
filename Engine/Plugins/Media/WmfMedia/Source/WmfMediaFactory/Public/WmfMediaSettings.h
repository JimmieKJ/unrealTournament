// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "WmfMediaSettings.generated.h"


UCLASS(config=Engine)
class WMFMEDIAFACTORY_API UWmfMediaSettings
	: public UObject
{
	GENERATED_BODY()

public:
	 
	/** Default constructor. */
	UWmfMediaSettings();

public:

	/**
	 * Whether to allow the loading of media that uses non-standard codecs.
	 *
	 * By default, the player will attempt to detect audio and video codecs that
	 * are not supported by the operating system out of the box, but may require
	 * the user to install additional codec packs. Enable this option to skip
	 * this check and allow the usage of non-standard codecs.
	 */
	UPROPERTY(config, EditAnywhere, Category=Media)
	bool AllowNonStandardCodecs;

	/** Play audio tracks via the operating system's native sound mixer. */
	UPROPERTY(config, EditAnywhere, Category=Debug)
	bool NativeAudioOut;
};
