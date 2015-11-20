// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneBoolTrack.h"
#include "MovieSceneBoolTrackInstance.h"


FMovieSceneBoolTrackInstance::FMovieSceneBoolTrackInstance( UMovieSceneBoolTrack& InBoolTrack )
{
	BoolTrack = &InBoolTrack;
	
	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( BoolTrack->GetPropertyName(), BoolTrack->GetPropertyPath() ) );
}


void FMovieSceneBoolTrackInstance::SaveState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for( UObject* Object : RuntimeObjects )
	{
		if (InitBoolMap.Find(Object) == nullptr)
		{
			bool BoolValue = PropertyBindings->GetCurrentValue<bool>(Object);
			InitBoolMap.Add(Object, BoolValue);
		}
	}
}


void FMovieSceneBoolTrackInstance::RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for( UObject* Object : RuntimeObjects )
	{
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


void FMovieSceneBoolTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) 
{
	bool BoolValue = false;

	if( BoolTrack->Eval( Position, LastPosition, BoolValue ) )
	{
		for( UObject* Object : RuntimeObjects )
		{
			PropertyBindings->CallFunction<bool>( Object, &BoolValue );
		}
	}
}


void FMovieSceneBoolTrackInstance::RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	PropertyBindings->UpdateBindings( RuntimeObjects );
}
