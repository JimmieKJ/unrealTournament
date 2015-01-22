// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieSceneMarginTrackInstance.h"
#include "MovieSceneMarginTrack.h"
#include "MovieSceneCommonHelpers.h"

FMovieSceneMarginTrackInstance::FMovieSceneMarginTrackInstance( UMovieSceneMarginTrack& InMarginTrack )
{
	MarginTrack = &InMarginTrack;

	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( MarginTrack->GetPropertyName(), MarginTrack->GetPropertyPath() ) );
}

void FMovieSceneMarginTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) 
{
	for( UObject* Object : RuntimeObjects )
	{
		FMargin MarginValue = PropertyBindings->GetCurrentValue<FMargin>( Object );
		if(MarginTrack->Eval(Position, LastPosition, MarginValue))
		{
			PropertyBindings->CallFunction(Object, &MarginValue);
		}
	}
}

void FMovieSceneMarginTrackInstance::RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player )
{
	PropertyBindings->UpdateBindings( RuntimeObjects );
}
