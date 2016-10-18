// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneStringTrack.h"
#include "MovieSceneStringTrackInstance.h"


/* FMovieSceneStringTrackInstance structors
 *****************************************************************************/

FMovieSceneStringTrackInstance::FMovieSceneStringTrackInstance(UMovieSceneStringTrack& InStringTrack)
{
	StringTrack = &InStringTrack;

	PropertyBindings = MakeShareable(new FTrackInstancePropertyBindings(StringTrack->GetPropertyName(), StringTrack->GetPropertyPath()));
}


/* IMovieSceneTrackInstance interface
 *****************************************************************************/

void FMovieSceneStringTrackInstance::RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	PropertyBindings->UpdateBindings(RuntimeObjects);
}


void FMovieSceneStringTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

		if (!IsValid(Object))
		{
			continue;
		}

		FString* StringValue = InitStringMap.Find(Object);

		if (StringValue != nullptr)
		{
			PropertyBindings->CallFunction<FString>(Object, StringValue);
		}
	}

	PropertyBindings->UpdateBindings(RuntimeObjects);
}


void FMovieSceneStringTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();

		if (InitStringMap.Find(Object) == nullptr)
		{
			const FString StringValue = PropertyBindings->GetCurrentValue<FString>(Object);
			InitStringMap.Add(Object, StringValue);
		}
	}
}


void FMovieSceneStringTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	FString StringValue;

	if (StringTrack->Eval(UpdateData.Position, UpdateData.LastPosition, StringValue))
	{
		for (auto ObjectPtr : RuntimeObjects)
		{
			UObject* Object = ObjectPtr.Get();
			PropertyBindings->CallFunction<FString>(Object, &StringValue);
		}
	}
}
