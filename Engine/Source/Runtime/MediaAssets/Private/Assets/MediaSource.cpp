// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaSource.h"
#include "UObject/SequencerObjectVersion.h"
#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#endif


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

	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);

	if (Ar.IsLoading() && (Ar.CustomVer(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::RenameMediaSourcePlatformPlayers))
	{
#if WITH_EDITORONLY_DATA
		if (!Ar.IsFilterEditorOnly())
		{
			TMap<FString, FString> DummyPlatformPlayers;
			Ar << DummyPlatformPlayers;
		}
#endif

		FString DummyDefaultPlayer;
		Ar << DummyDefaultPlayer;
	}
	else
	{
#if WITH_EDITORONLY_DATA
		if (Ar.IsFilterEditorOnly())
		{
			if (Ar.IsSaving())
			{
				const FName* PlatformPlayerName = PlatformPlayerNames.Find(Ar.CookingTarget()->PlatformName());
				PlayerName = (PlatformPlayerName != nullptr) ? *PlatformPlayerName : NAME_None;
			}

			Ar << PlayerName;
		}
		else
		{
			Ar << PlatformPlayerNames;
		}
#else
		Ar << PlayerName;
#endif
	}
}


/* IMediaOptions interface
 *****************************************************************************/

FName UMediaSource::GetDesiredPlayerName() const
{
#if WITH_EDITORONLY_DATA
	const FString RunningPlatformName(FPlatformProperties::IniPlatformName());
	const FName* PlatformPlayerName = PlatformPlayerNames.Find(RunningPlatformName);

	if (PlatformPlayerName == nullptr)
	{
		return NAME_None;
	}

	return *PlatformPlayerName;
#else
	return PlayerName;
#endif
}


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
