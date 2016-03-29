// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSlomoTrack.h"
#include "MovieSceneSlomoTrackInstance.h"


/* FMovieSceneSlomoTrackInstance structors
 *****************************************************************************/

FMovieSceneSlomoTrackInstance::FMovieSceneSlomoTrackInstance(UMovieSceneSlomoTrack& InSlomoTrack)
	: SlomoTrack(&InSlomoTrack)
	, InitMatineeTimeDilation(1.0f)
{ }

	
void FMovieSceneSlomoTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	AWorldSettings* WorldSettings = GWorld->GetWorldSettings();

	if (WorldSettings == nullptr)
	{
		return;
	}

	WorldSettings->MatineeTimeDilation = InitMatineeTimeDilation;
}

void FMovieSceneSlomoTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	AWorldSettings* WorldSettings = GWorld->GetWorldSettings();

	if (WorldSettings == nullptr)
	{
		return;
	}

	InitMatineeTimeDilation = WorldSettings->MatineeTimeDilation;
}

/* IMovieSceneTrackInstance interface
 *****************************************************************************/

void FMovieSceneSlomoTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
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

	if (SlomoTrack->Eval(UpdateData.Position, UpdateData.LastPosition, FloatValue))
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
