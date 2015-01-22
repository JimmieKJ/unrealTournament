// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieScene2DTransformTrackInstance.h"
#include "MovieScene2DTransformTrack.h"
#include "MovieSceneCommonHelpers.h"

FMovieScene2DTransformTrackInstance::FMovieScene2DTransformTrackInstance( UMovieScene2DTransformTrack& InTransformTrack )
	: TransformTrack( &InTransformTrack )
{
	PropertyBindings = MakeShareable(new FTrackInstancePropertyBindings(TransformTrack->GetPropertyName(), TransformTrack->GetPropertyPath()));
}

void FMovieScene2DTransformTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) 
{
	for(UObject* Object : RuntimeObjects)
	{
		FWidgetTransform TransformValue = PropertyBindings->GetCurrentValue<FWidgetTransform>(Object);
		if(TransformTrack->Eval(Position, LastPosition, TransformValue))
		{
			PropertyBindings->CallFunction(Object, &TransformValue);
		}
	}
}

void FMovieScene2DTransformTrackInstance::RefreshInstance(const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player)
{
	PropertyBindings->UpdateBindings(RuntimeObjects);
}

