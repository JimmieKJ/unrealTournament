// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPCH.h"
#include "MediaSource.h"
#include "PlatformMediaSource.h"


/* UMediaSource interface
 *****************************************************************************/

FString UPlatformMediaSource::GetUrl() const
{
	return (DefaultSource != nullptr) ? DefaultSource->GetUrl() : FString();
}


void UPlatformMediaSource::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);


}


bool UPlatformMediaSource::Validate() const
{
	return (DefaultSource != nullptr) ? DefaultSource->Validate() : false;
}


/* IMediaOptions interface
 *****************************************************************************/

bool UPlatformMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	return (DefaultSource != nullptr) ? DefaultSource->GetMediaOption(Key, DefaultValue) : DefaultValue;
}


double UPlatformMediaSource::GetMediaOption(const FName& Key, double DefaultValue) const
{
	return (DefaultSource != nullptr) ? DefaultSource->GetMediaOption(Key, DefaultValue) : DefaultValue;
}


int64 UPlatformMediaSource::GetMediaOption(const FName& Key, int64 DefaultValue) const
{
	return (DefaultSource != nullptr) ? DefaultSource->GetMediaOption(Key, DefaultValue) : DefaultValue;
}


FString UPlatformMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	return (DefaultSource != nullptr) ? DefaultSource->GetMediaOption(Key, DefaultValue) : DefaultValue;
}


FText UPlatformMediaSource::GetMediaOption(const FName& Key, const FText& DefaultValue) const
{
	return (DefaultSource != nullptr) ? DefaultSource->GetMediaOption(Key, DefaultValue) : DefaultValue;
}


bool UPlatformMediaSource::HasMediaOption(const FName& Key) const
{
	return (DefaultSource != nullptr) ? DefaultSource->HasMediaOption(Key) : false;
}
