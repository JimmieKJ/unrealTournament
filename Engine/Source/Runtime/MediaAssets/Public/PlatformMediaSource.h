// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaSource.h"
#include "PlatformMediaSource.generated.h"


/**
 * A media source that selects other media sources based on target platform.
 *
 * Use this asset to override media sources on a per-platform basis.
 */
UCLASS(BlueprintType)
class MEDIAASSETS_API UPlatformMediaSource
	: public UMediaSource
{
	GENERATED_BODY()

public:

	/**
	 * Default media source.
	 *
	 * This media source will be used if no source was specified for a target platform.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Sources)
	UMediaSource* DefaultSource;

#if WITH_EDITORONLY_DATA

	/** Media sources per platform. */
	UPROPERTY(EditAnywhere, Category=Sources)
	TMap<FString, UMediaSource*> MediaSources;

#endif

public:

	//~ UMediaSource interface

	virtual FString GetUrl() const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual bool Validate() const override;

public:

	//~ IMediaOptions interface

	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual double GetMediaOption(const FName& Key, double DefaultValue) const override;
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const override;
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const override;
	virtual FText GetMediaOption(const FName& Key, const FText& DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;
};
