// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneByteTrack.h"
#include "MovieSceneByteTrackInstance.h"


FMovieSceneByteTrackInstance::FMovieSceneByteTrackInstance( UMovieSceneByteTrack& InByteTrack )
{
	ByteTrack = &InByteTrack;
	
	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( ByteTrack->GetPropertyName(), ByteTrack->GetPropertyPath() ) );
}


void FMovieSceneByteTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

		if (InitByteMap.Find(Object) == nullptr)
		{
			uint8 ByteValue = PropertyBindings->GetCurrentValue<uint8>(Object);
			InitByteMap.Add(Object, ByteValue);
		}
	}
}


void FMovieSceneByteTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

		if (!IsValid(Object))
		{
			continue;
		}

		uint8 *ByteValue = InitByteMap.Find(Object);
		if (ByteValue != nullptr)
		{
			PropertyBindings->CallFunction<uint8>(Object, ByteValue);
		}
	}

	PropertyBindings->UpdateBindings( RuntimeObjects );
}


void FMovieSceneByteTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	uint8 ByteValue = 0;
	if( ByteTrack->Eval( UpdateData.Position, UpdateData.LastPosition, ByteValue ) )
	{
		for (auto ObjectPtr : RuntimeObjects )
		{
			UObject* Object = ObjectPtr.Get();
			PropertyBindings->CallFunction<uint8>(Object, &ByteValue);
		}
	}
}


void FMovieSceneByteTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	PropertyBindings->UpdateBindings( RuntimeObjects );
}
