// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaPrivatePCH.h"


#define LOCTEXT_NAMESPACE "FAvfMediaTrack"


/* IMediaTrack interface
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
		DisplayName = FText::Format(LOCTEXT("UnnamedTrackFormat", "Unnamed Track {0}"), FText::AsNumber((uint32)StreamIndex));
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


uint32 FAvfMediaTrack::GetIndex() const
{
	return StreamIndex;
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


bool FAvfMediaTrack::IsMutuallyExclusive( const IMediaTrackRef& Other ) const
{
	return false;
}


bool FAvfMediaTrack::IsProtected() const
{
	return Protected;
}


#undef LOCTEXT_NAMESPACE
