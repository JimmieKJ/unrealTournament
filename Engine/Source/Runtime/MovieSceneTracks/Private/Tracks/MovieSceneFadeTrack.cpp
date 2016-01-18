// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneFadeSection.h"
#include "MovieSceneFadeTrack.h"
#include "MovieSceneFadeTrackInstance.h"


#define LOCTEXT_NAMESPACE "MovieSceneFadeTrack"


/* UMovieSceneFadeTrack interface
 *****************************************************************************/

TSharedPtr<IMovieSceneTrackInstance> UMovieSceneFadeTrack::CreateInstance()
{
	return MakeShareable(new FMovieSceneFadeTrackInstance(*this)); 
}


UMovieSceneSection* UMovieSceneFadeTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneFadeSection::StaticClass(), NAME_None, RF_Transactional);
}


#if WITH_EDITORONLY_DATA
FText UMovieSceneFadeTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Fade");
}
#endif


#undef LOCTEXT_NAMESPACE
