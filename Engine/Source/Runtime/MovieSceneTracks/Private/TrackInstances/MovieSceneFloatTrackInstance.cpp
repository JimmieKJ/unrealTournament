// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneFloatTrack.h"
#include "MovieSceneFloatTrackInstance.h"


FMovieSceneFloatTrackInstance::FMovieSceneFloatTrackInstance( UMovieSceneFloatTrack& InFloatTrack )
{
	FloatTrack = &InFloatTrack;

	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( FloatTrack->GetPropertyName(), FloatTrack->GetPropertyPath() ) );
}


void FMovieSceneFloatTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

		if (InitFloatMap.Find(Object) == nullptr)
		{
			float FloatValue = PropertyBindings->GetCurrentValue<float>(Object);
			InitFloatMap.Add(Object, FloatValue);
		}
	}
}


void FMovieSceneFloatTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

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


void FMovieSceneFloatTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) 
{
	float FloatValue = 0.0f;
	if( FloatTrack->Eval( UpdateData.Position, UpdateData.LastPosition, FloatValue ) )
	{
		for(auto ObjectPtr : RuntimeObjects)
		{
			UObject* Object = ObjectPtr.Get();
			PropertyBindings->CallFunction<float>( Object, &FloatValue );
		}
	}
}


void FMovieSceneFloatTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	PropertyBindings->UpdateBindings( RuntimeObjects );
}
