// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPCH.h"
#include "FileMediaSource.h"


static FName PrecacheFileName("PrecacheFile");


bool UFileMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	if (Key == PrecacheFileName)
	{
		return PrecacheFile;
	}

	return DefaultValue;
}


FString UFileMediaSource::GetUrl() const
{
	return FString(TEXT("file://")) + GetFullPath();
}


bool UFileMediaSource::Validate() const
{
	return FPaths::FileExists(GetFullPath());
}


FString UFileMediaSource::GetFullPath() const
{
	FString FullPath = FPaths::ConvertRelativePathToFull(
		FPaths::IsRelative(FilePath)
			? FPaths::GameContentDir() / FilePath
			: FilePath
	);

	return FullPath;
}
