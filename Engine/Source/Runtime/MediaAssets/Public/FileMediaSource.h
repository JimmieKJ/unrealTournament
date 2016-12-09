// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "MediaSource.h"
#include "FileMediaSource.generated.h"

UCLASS(BlueprintType)
class MEDIAASSETS_API UFileMediaSource
	: public UMediaSource
{
	GENERATED_BODY()

public:

	/**
	 * The path to the media file to be played.
	 *
	 * @see SetFilePath
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=File, AssetRegistrySearchable)
	FString FilePath;

	/** Load entire media file into memory and play from there (if possible). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=File, AdvancedDisplay)
	bool PrecacheFile;

public:

	/**
	 * Get the path to the media file to be played.
	 *
	 * @return The file path.
	 * @see SetFilePath
	 */
	const FString& GetFilePath() const
	{
		return FilePath;
	}

	/**
	 * Set the path to the media file that this source represents.
	 *
	 * Automatically converts full paths to media sources that reside in the
	 * Engine's or project's /Content/Movies directory into relative paths.
	 *
	 * @param Path The path to set.
	 * @see FilePath, GetFilePath
	 */
	UFUNCTION(BlueprintCallable, Category="Media|FileMediaSource")
	void SetFilePath(const FString& Path);

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
