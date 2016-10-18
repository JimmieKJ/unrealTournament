// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPCH.h"
#include "MediaSource.h"


/* UMediaSource interface
 *****************************************************************************/

FString UMediaSource::GetDesiredPlayer() const
{
#if WITH_EDITORONLY_DATA
	const FString RunningPlatformName(FPlatformProperties::IniPlatformName());
	const FString* PlatformPlayer = PlatformPlayers.Find(RunningPlatformName);

	if (PlatformPlayer != nullptr)
	{
		return *PlatformPlayer;
	}
#endif

	return DefaultPlayer;
}


/* UObject interface
 *****************************************************************************/

void UMediaSource::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	FString Url = GetUrl();

	if (!Url.IsEmpty())
	{
		OutTags.Add(FAssetRegistryTag("Url", Url, FAssetRegistryTag::TT_Alphabetical));
	}
}


#if WITH_EDITOR
void UMediaSource::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
}
#endif


void UMediaSource::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (Ar.IsCooking())
	{
		const FString* PlatformPlayer = PlatformPlayers.Find(Ar.CookingTarget()->PlatformName());

		if (PlatformPlayer != nullptr)
		{
			DefaultPlayer = *PlatformPlayer;
		}
	}
	else
	{
		Ar << PlatformPlayers;
	}
#endif

	Ar << DefaultPlayer;
}


/* IMediaOptions interface
 *****************************************************************************/

bool UMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	return DefaultValue;
}


double UMediaSource::GetMediaOption(const FName& Key, double DefaultValue) const
{
	return DefaultValue;
}


int64 UMediaSource::GetMediaOption(const FName& Key, int64 DefaultValue) const
{
	return DefaultValue;
}


FString UMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	return DefaultValue;
}


FText UMediaSource::GetMediaOption(const FName& Key, const FText& DefaultValue) const
{
	return DefaultValue;
}


bool UMediaSource::HasMediaOption(const FName& Key) const
{
	return false;
}
