// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieScenePrivatePCH.h"
#include "MovieSceneNameableTrack.h"


#define LOCTEXT_NAMESPACE "MovieSceneNameableTrack"


/* UMovieSceneNameableTrack interface
 *****************************************************************************/

#if WITH_EDITORONLY_DATA

void UMovieSceneNameableTrack::SetDisplayName(const FText& NewDisplayName)
{
	if (NewDisplayName.EqualTo(DisplayName))
	{
		return;
	}

	SetFlags(RF_Transactional);
	Modify();

	DisplayName = NewDisplayName;
}

#endif


/* UMovieSceneTrack interface
 *****************************************************************************/

#if WITH_EDITORONLY_DATA

FText UMovieSceneNameableTrack::GetDisplayName() const
{
	if (DisplayName.IsEmpty())
	{
		return LOCTEXT("UnnamedTrackName", "Unnamed Track");
	}

	return DisplayName;
}

#endif


#undef LOCTEXT_NAMESPACE
