// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaSource.h"
#include "FileMediaSource.generated.h"


UCLASS(BlueprintType)
class MEDIAASSETS_API UFileMediaSource
	: public UMediaSource
{
	GENERATED_BODY()

public:

	/** The path to the media file to be played. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=File, AssetRegistrySearchable)
	FString FilePath;

	/** Load entire media file into memory and play from there (if possible). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=File)
	bool PrecacheFile;

public:

	//~ IMediaOptions interface

	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;

public:

	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual bool Validate() const override;

protected:

	/** Get the full path to the file. */
	FString GetFullPath() const;
};
