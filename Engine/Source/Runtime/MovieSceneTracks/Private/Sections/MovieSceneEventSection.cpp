// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneEventSection.h"


/* UMovieSceneSection structors
 *****************************************************************************/

UMovieSceneEventSection::UMovieSceneEventSection()
{
	SetIsInfinite(true);
}


/* UMovieSceneSection overrides
 *****************************************************************************/

void UMovieSceneEventSection::AddKey(float Time, const FName& EventName, FKeyParams KeyParams)
{
	Modify();
	Events.UpdateOrAddKey(Time, EventName);
}


void UMovieSceneEventSection::TriggerEvents(ALevelScriptActor* LevelScriptActor, float Position, float LastPosition)
{
	const TArray<FNameCurveKey>& Keys = Events.GetKeys();

	if (Position >= LastPosition)
	{
		for (const auto& Key : Keys)
		{
			if ((Key.Time > LastPosition) && (Key.Time <= Position))
			{
				TriggerEvent(Key.Value, LevelScriptActor);
			}
		}
	}
	else
	{
		for (int32 KeyIndex = Keys.Num() - 1; KeyIndex >= 0; --KeyIndex)
		{
			const auto& Key = Keys[KeyIndex];

			if ((Key.Time >= Position) && (Key.Time < LastPosition))
			{
				TriggerEvent(Key.Value, LevelScriptActor);
			}

			
		}
	}
}


/* UMovieSceneSection overrides
 *****************************************************************************/

void UMovieSceneEventSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	Events.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneEventSection::GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const
{
	for (auto It(Events.GetKeyHandleIterator()); It; ++It)
	{
		float Time = Events.GetKeyTime(It.Key());
		if (IsTimeWithinSection(Time))
		{
			KeyHandles.Add(It.Key());
		}
	}
}


void UMovieSceneEventSection::MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles)
{
	Super::MoveSection(DeltaPosition, KeyHandles);

	Events.ShiftCurve(DeltaPosition, KeyHandles);
}


/* UMovieSceneSection overrides
 *****************************************************************************/

void UMovieSceneEventSection::TriggerEvent(const FName& Event, ALevelScriptActor* LevelScriptActor)
{
	UFunction* EventFunction = LevelScriptActor->FindFunction(Event);

	if (EventFunction == nullptr)
	{
		// @todo sequencer: gmp: add external log category for MovieScene
		//UE_LOG(LogMovieScene, Log, TEXT("UMovieSceneEventSection::TriggerEvent: Unable to find function '%s'"), *Event.ToString());
	}
	else if (EventFunction->NumParms != 0)
	{
		// @todo sequencer: gmp: add external log category for MovieScene
		//UE_LOG(LogMovieScene, Log, TEXT("UMovieSceneEventSection::TriggerEvent: Function '%s' does not have zero parameters."), *Event.ToString());
	}
	else
	{
		LevelScriptActor->ProcessEvent(EventFunction, nullptr);
	}
}
