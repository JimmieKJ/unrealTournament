// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PlatformMediaSource.h"
#include "UObject/SequencerObjectVersion.h"
#if WITH_EDITOR
	#include "Interfaces/ITargetPlatform.h"
#endif

/* UMediaSource interface
 *****************************************************************************/

FString UPlatformMediaSource::GetUrl() const
{
	return (MediaSource != nullptr) ? MediaSource->GetUrl() : FString();
}


void UPlatformMediaSource::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);

#if WITH_EDITORONLY_DATA
	if (Ar.IsCooking())
	{
		UMediaSource** PlatformMediaSource = PlatformMediaSources.Find(Ar.CookingTarget()->PlatformName());
		MediaSource = (PlatformMediaSource != nullptr) ? *PlatformMediaSource : nullptr;
	}
#endif
	
	if (Ar.IsLoading() && (Ar.CustomVer(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::RenameMediaSourcePlatformPlayers))
	{
		FString DummyDefaultSource;
		Ar << DummyDefaultSource;

#if WITH_EDITORONLY_DATA
		Ar << PlatformMediaSources;
#endif
	}
	else
	{
#if WITH_EDITORONLY_DATA
		Ar << PlatformMediaSources;
#else
		Ar << MediaSource;
#endif
	}
}


bool UPlatformMediaSource::Validate() const
{
	return (MediaSource != nullptr) ? MediaSource->Validate() : false;
}


/* UPlatformMediaSource implementation
 *****************************************************************************/

UMediaSource* UPlatformMediaSource::GetMediaSource() const
{
#if WITH_EDITORONLY_DATA
	const FString RunningPlatformName(FPlatformProperties::IniPlatformName());
	UMediaSource*const* PlatformMediaSource = PlatformMediaSources.Find(RunningPlatformName);

	if (PlatformMediaSource == nullptr)
	{
		return nullptr;
	}

	return *PlatformMediaSource;
#else
	return MediaSource;
#endif
}


/* IMediaOptions interface
 *****************************************************************************/

bool UPlatformMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	UMediaSource* PlatformMediaSource = GetMediaSource();
	return (PlatformMediaSource != nullptr) ? PlatformMediaSource->GetMediaOption(Key, DefaultValue) : DefaultValue;
}


double UPlatformMediaSource::GetMediaOption(const FName& Key, double DefaultValue) const
{
	UMediaSource* PlatformMediaSource = GetMediaSource();
	return (PlatformMediaSource != nullptr) ? PlatformMediaSource->GetMediaOption(Key, DefaultValue) : DefaultValue;
}


int64 UPlatformMediaSource::GetMediaOption(const FName& Key, int64 DefaultValue) const
{
	UMediaSource* PlatformMediaSource = GetMediaSource();
	return (PlatformMediaSource != nullptr) ? PlatformMediaSource->GetMediaOption(Key, DefaultValue) : DefaultValue;
}


FString UPlatformMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	UMediaSource* PlatformMediaSource = GetMediaSource();
	return (PlatformMediaSource != nullptr) ? PlatformMediaSource->GetMediaOption(Key, DefaultValue) : DefaultValue;
}


FText UPlatformMediaSource::GetMediaOption(const FName& Key, const FText& DefaultValue) const
{
	UMediaSource* PlatformMediaSource = GetMediaSource();
	return (PlatformMediaSource != nullptr) ? PlatformMediaSource->GetMediaOption(Key, DefaultValue) : DefaultValue;
}


bool UPlatformMediaSource::HasMediaOption(const FName& Key) const
{
	UMediaSource* PlatformMediaSource = GetMediaSource();
	return (PlatformMediaSource != nullptr) ? PlatformMediaSource->HasMediaOption(Key) : false;
}
