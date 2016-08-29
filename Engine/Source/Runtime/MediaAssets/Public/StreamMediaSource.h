// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaSource.h"
#include "StreamMediaSource.generated.h"


UCLASS(BlueprintType)
class MEDIAASSETS_API UStreamMediaSource
	: public UMediaSource
{
	GENERATED_BODY()

public:

	/** The URL to the media stream to be played. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Stream, AssetRegistrySearchable)
	FString StreamUrl;

public:

	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual bool Validate() const override;
};
