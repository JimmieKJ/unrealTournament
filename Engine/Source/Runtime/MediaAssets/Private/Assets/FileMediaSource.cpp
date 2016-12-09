// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FileMediaSource.h"
#include "Misc/Paths.h"


static FName PrecacheFileName("PrecacheFile");


/* UFileMediaSource interface
 *****************************************************************************/

void UFileMediaSource::SetFilePath(const FString& Path)
{
	if (Path.IsEmpty() || Path.StartsWith(TEXT("./")))
	{
		FilePath = Path;
	}
	else
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(Path);
		const FString FullGameContentDir = FPaths::ConvertRelativePathToFull(FPaths::GameContentDir());

		if (FullPath.StartsWith(FullGameContentDir))
		{
			FPaths::MakePathRelativeTo(FullPath, *FullGameContentDir);
			FullPath = FString(TEXT("./")) + FullPath;
		}

		FilePath = FullPath;
	}
}


/* UMediaSource overrides
 *****************************************************************************/

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


/* UFileMediaSource implementation
 *****************************************************************************/

FString UFileMediaSource::GetFullPath() const
{
	if (!FPaths::IsRelative(FilePath))
	{
		return FilePath;
	}

	if (FilePath.StartsWith(TEXT("./")))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::GameContentDir(), FilePath.RightChop(2));
	}

	return FPaths::ConvertRelativePathToFull(FilePath);
}
