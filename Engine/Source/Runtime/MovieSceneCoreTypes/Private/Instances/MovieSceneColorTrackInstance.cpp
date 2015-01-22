// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneColorTrackInstance.h"
#include "MovieSceneCommonHelpers.h"


FMovieSceneColorTrackInstance::FMovieSceneColorTrackInstance( UMovieSceneColorTrack& InColorTrack )
{
	ColorTrack = &InColorTrack;

	PropertyBindings = MakeShareable( new FTrackInstancePropertyBindings( ColorTrack->GetPropertyName(), ColorTrack->GetPropertyPath() ) );
}

void FMovieSceneColorTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) 
{
	for(UObject* Object : RuntimeObjects)
	{
		if( ColorTrack->IsSlateColor() )
		{
			FSlateColor ColorValue = PropertyBindings->GetCurrentValue<FSlateColor>(Object);
			FLinearColor LinearColor = ColorValue.GetSpecifiedColor();
			if(ColorTrack->Eval(Position, LastPosition, LinearColor))
			{
				FSlateColor NewColor(LinearColor);
				PropertyBindings->CallFunction(Object, &NewColor);
			}
		}
		else
		{
			FLinearColor ColorValue = PropertyBindings->GetCurrentValue<FLinearColor>(Object);
			if(ColorTrack->Eval(Position, LastPosition, ColorValue))
			{
				PropertyBindings->CallFunction(Object, &ColorValue);
			}
		}
	}
}

void FMovieSceneColorTrackInstance::RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player )
{
	PropertyBindings->UpdateBindings( RuntimeObjects );
}
