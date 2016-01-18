// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaPrivatePCH.h"


#define LOCTEXT_NAMESPACE "FAvfMediaTrack"


/* IMediaStream interface
 *****************************************************************************/

bool FAvfMediaTrack::Disable()
{
    return false;
}


bool FAvfMediaTrack::Enable()
{
    return false;
}


FText FAvfMediaTrack::GetDisplayName() const
{
	FText DisplayName;

	if (Name.IsEmpty())
	{
		DisplayName = FText::Format(LOCTEXT("UnnamedTrackFormat", "Unnamed Stream {0}"), FText::AsNumber((uint32)StreamIndex));
	}
	else
	{
		DisplayName = FText::FromString(Name);
	}

	if (Language.IsEmpty())
	{
		return DisplayName;
	}

	return FText::Format(LOCTEXT("LocalizedTrackFormat", "{0} ({1})"), DisplayName, FText::FromString(Language));
}


FString FAvfMediaTrack::GetLanguage() const
{
	return Language;
}


FString FAvfMediaTrack::GetName() const
{
	return Name;
}


bool FAvfMediaTrack::IsEnabled() const
{
	bool bSelected = true;
	return bSelected;
}


bool FAvfMediaTrack::IsMutuallyExclusive( const IMediaStreamRef& Other ) const
{
	return false;
}


bool FAvfMediaTrack::IsProtected() const
{
	return Protected;
}


#undef LOCTEXT_NAMESPACE
