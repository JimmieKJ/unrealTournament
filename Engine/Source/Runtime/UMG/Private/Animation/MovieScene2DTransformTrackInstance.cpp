// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieScene2DTransformTrackInstance.h"
#include "MovieScene2DTransformTrack.h"
#include "MovieSceneCommonHelpers.h"


FMovieScene2DTransformTrackInstance::FMovieScene2DTransformTrackInstance( UMovieScene2DTransformTrack& InTransformTrack )
	: TransformTrack( &InTransformTrack )
{
	PropertyBindings = MakeShareable(new FTrackInstancePropertyBindings(TransformTrack->GetPropertyName(), TransformTrack->GetPropertyPath()));
}


void FMovieScene2DTransformTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();
		FWidgetTransform TransformValue = PropertyBindings->GetCurrentValue<FWidgetTransform>(Object);

		if(TransformTrack->Eval(UpdateData.Position, UpdateData.LastPosition, TransformValue))
		{
			PropertyBindings->CallFunction<FWidgetTransform>(Object, &TransformValue);
		}
	}
}


void FMovieScene2DTransformTrackInstance::RefreshInstance(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	PropertyBindings->UpdateBindings(RuntimeObjects);
}
