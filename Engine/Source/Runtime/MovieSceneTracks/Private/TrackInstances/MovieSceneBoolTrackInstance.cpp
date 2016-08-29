// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneBoolTrack.h"
#include "MovieSceneBoolTrackInstance.h"


FMovieSceneBoolTrackInstance::FMovieSceneBoolTrackInstance( UMovieSceneBoolTrack& InBoolTrack )
{
	BoolTrack = &InBoolTrack;
	
	FString PropertyVarName = BoolTrack->GetPropertyName().ToString();
	PropertyVarName.RemoveFromStart("b", ESearchCase::CaseSensitive);
	FName PropertyName = FName(*PropertyVarName);

	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( PropertyName, BoolTrack->GetPropertyPath() ) );
}


void FMovieSceneBoolTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

		if (InitBoolMap.Find(Object) == nullptr)
		{
			bool BoolValue = PropertyBindings->GetCurrentValue<bool>(Object);
			InitBoolMap.Add(Object, BoolValue);
		}
	}
}


void FMovieSceneBoolTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

		if (!IsValid(Object))
		{
			continue;
		}

		bool *BoolValue = InitBoolMap.Find(Object);

		if (BoolValue != nullptr)
		{
			PropertyBindings->CallFunction<bool>(Object, BoolValue);
		}
	}

	PropertyBindings->UpdateBindings( RuntimeObjects );
}


void FMovieSceneBoolTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	bool BoolValue = false;

	if( BoolTrack->Eval( UpdateData.Position, UpdateData.LastPosition, BoolValue ) )
	{
		for (auto ObjectPtr : RuntimeObjects)
		{
			UObject* Object = ObjectPtr.Get();
			PropertyBindings->CallFunction<bool>( Object, &BoolValue );
		}
	}
}


void FMovieSceneBoolTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	PropertyBindings->UpdateBindings( RuntimeObjects );
}
