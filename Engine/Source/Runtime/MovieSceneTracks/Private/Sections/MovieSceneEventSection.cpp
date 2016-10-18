// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	if (TryModify())
	{
		Events.UpdateOrAddKey(Time, EventName);
	}
}


void UMovieSceneEventSection::TriggerEvents(float Position, float LastPosition,  IMovieScenePlayer& Player)
{
	const TArray<FNameCurveKey>& Keys = Events.GetKeys();

	if (Position >= LastPosition)
	{
		for (const auto& Key : Keys)
		{
			if ((Key.Time >= LastPosition) && (Key.Time <= Position))
			{
				TriggerEvent(Key.Value, Position, Player);
			}
		}
	}
	else
	{
		for (int32 KeyIndex = Keys.Num() - 1; KeyIndex >= 0; --KeyIndex)
		{
			const auto& Key = Keys[KeyIndex];

			if ((Key.Time >= Position) && (Key.Time <= LastPosition))
			{
				TriggerEvent(Key.Value, Position, Player);
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


void UMovieSceneEventSection::GetKeyHandles(TSet<FKeyHandle>& KeyHandles, TRange<float> TimeRange) const
{
	for (auto It(Events.GetKeyHandleIterator()); It; ++It)
	{
		float Time = Events.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
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


TOptional<float> UMovieSceneEventSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	if ( Events.IsKeyHandleValid( KeyHandle ) )
	{
		return TOptional<float>( Events.GetKeyTime( KeyHandle ) );
	}
	return TOptional<float>();
}


void UMovieSceneEventSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	if ( Events.IsKeyHandleValid( KeyHandle ) )
	{
		Events.SetKeyTime( KeyHandle, Time );
	}
}


/* UMovieSceneSection overrides
 *****************************************************************************/

void UMovieSceneEventSection::TriggerEvent(const FName& Event, float Position, IMovieScenePlayer& Player)
{
	for (UObject* EventContextObject : Player.GetEventContexts())
	{
		UFunction* EventFunction = EventContextObject->FindFunction(Event);

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
			EventContextObject->ProcessEvent(EventFunction, nullptr);
		}
	}

#if !UE_BUILD_SHIPPING
	if (Event == NAME_PerformanceCapture)
	{
		FString PackageName = GetOutermost()->GetName();
		
		FString LevelSequenceName;
		FString FolderName;
		PackageName.Split(TEXT("/"), &FolderName, &LevelSequenceName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		UWorld* PlaybackContext = Cast<UWorld>(Player.GetPlaybackContext());

		FString MapName = PlaybackContext->GetName();

		GEngine->PerformanceCapture(PlaybackContext, MapName, LevelSequenceName, Position);
	}
#endif	// UE_BUILD_SHIPPING
}
