// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneFloatTrack.h"
#include "MovieSceneFloatTrackInstance.h"


FMovieSceneFloatTrackInstance::FMovieSceneFloatTrackInstance( UMovieSceneFloatTrack& InFloatTrack )
{
	FloatTrack = &InFloatTrack;

	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( FloatTrack->GetPropertyName(), FloatTrack->GetPropertyPath() ) );
}


void FMovieSceneFloatTrackInstance::SaveState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for( UObject* Object : RuntimeObjects )
	{
		if (InitFloatMap.Find(Object) == nullptr)
		{
			float FloatValue = PropertyBindings->GetCurrentValue<float>(Object);
			InitFloatMap.Add(Object, FloatValue);
		}
	}
}


void FMovieSceneFloatTrackInstance::RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for( UObject* Object : RuntimeObjects )
	{
		if (!IsValid(Object))
		{
			continue;
		}

		float *FloatValue = InitFloatMap.Find(Object);
		if (FloatValue != nullptr)
		{
			PropertyBindings->CallFunction<float>(Object, FloatValue);
		}
	}

	PropertyBindings->UpdateBindings( RuntimeObjects );
}


void FMovieSceneFloatTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance, EMovieSceneUpdatePass UpdatePass ) 
{
	float FloatValue = 0.0f;
	if( FloatTrack->Eval( Position, LastPosition, FloatValue ) )
	{
		for(UObject* Object : RuntimeObjects)
		{
			PropertyBindings->CallFunction<float>( Object, &FloatValue );
		}
	}
}


void FMovieSceneFloatTrackInstance::RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	PropertyBindings->UpdateBindings( RuntimeObjects );
}
