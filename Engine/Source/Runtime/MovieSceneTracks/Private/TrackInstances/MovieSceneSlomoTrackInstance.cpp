// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSlomoTrack.h"
#include "MovieSceneSlomoTrackInstance.h"


/* FMovieSceneSlomoTrackInstance structors
 *****************************************************************************/

FMovieSceneSlomoTrackInstance::FMovieSceneSlomoTrackInstance(UMovieSceneSlomoTrack& InSlomoTrack)
	: SlomoTrack(&InSlomoTrack)
{ }


/* IMovieSceneTrackInstance interface
 *****************************************************************************/

void FMovieSceneSlomoTrackInstance::Update(float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass)
{
	if (!ShouldBeApplied())
	{
		return;
	}

	AWorldSettings* WorldSettings = GWorld->GetWorldSettings();

	if (WorldSettings == nullptr)
	{
		return;
	}

	float FloatValue = 0.0f;

	if (SlomoTrack->Eval(Position, LastPosition, FloatValue))
	{
		WorldSettings->MatineeTimeDilation = FloatValue;
		WorldSettings->ForceNetUpdate();
	}
}


/* IMovieSceneTrackInstance implementation
 *****************************************************************************/

bool FMovieSceneSlomoTrackInstance::ShouldBeApplied() const
{
	if (GIsEditor)
	{
		return true;
	}

	if (GWorld->GetNetMode() == NM_Client)
	{
		return false;
	}

	return (GEngine != nullptr);
}
