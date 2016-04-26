// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieSceneMarginTrackInstance.h"
#include "MovieSceneMarginTrack.h"
#include "MovieSceneCommonHelpers.h"


FMovieSceneMarginTrackInstance::FMovieSceneMarginTrackInstance( UMovieSceneMarginTrack& InMarginTrack )
{
	MarginTrack = &InMarginTrack;

	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( MarginTrack->GetPropertyName(), MarginTrack->GetPropertyPath() ) );
}


void FMovieSceneMarginTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	for (auto ObjectPtr : RuntimeObjects)
	{
		UObject* Object = ObjectPtr.Get();
		FMargin MarginValue = PropertyBindings->GetCurrentValue<FMargin>(Object);

		if (MarginTrack->Eval(UpdateData.Position, UpdateData.LastPosition, MarginValue))
		{
			PropertyBindings->CallFunction<FMargin>(Object, &MarginValue);
		}
	}
}


void FMovieSceneMarginTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	PropertyBindings->UpdateBindings( RuntimeObjects );
}
