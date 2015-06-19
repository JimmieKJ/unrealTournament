// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "MovieScene2DTransformSection.h"
#include "MovieScene2DTransformTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieScene2DTransformTrackInstance.h"
#include "MovieSceneCommonHelpers.h"

UMovieScene2DTransformTrack::UMovieScene2DTransformTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

UMovieSceneSection* UMovieScene2DTransformTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieScene2DTransformSection::StaticClass(), NAME_None, RF_Transactional);
}

TSharedPtr<IMovieSceneTrackInstance> UMovieScene2DTransformTrack::CreateInstance()
{
	return MakeShareable( new FMovieScene2DTransformTrackInstance( *this ) );
}

bool UMovieScene2DTransformTrack::Eval(float Position, float LastPosition, FWidgetTransform& InOutTransform) const
{
	const UMovieSceneSection* Section = MovieSceneHelpers::FindSectionAtTime(Sections, Position);

	if(Section)
	{
		const UMovieScene2DTransformSection* TransformSection = CastChecked<UMovieScene2DTransformSection>(Section);

		InOutTransform = TransformSection->Eval(Position, InOutTransform);
	}

	return Section != NULL;
}


bool UMovieScene2DTransformTrack::AddKeyToSection(float Time, const F2DTransformKey& TransformKey)
{
	const UMovieSceneSection* NearestSection = MovieSceneHelpers::FindSectionAtTime(Sections, Time);
	if(!NearestSection || TransformKey.bAddKeyEvenIfUnchanged || CastChecked<UMovieScene2DTransformSection>(NearestSection)->NewKeyIsNewData(Time, TransformKey.Value))
	{
		Modify();

		UMovieScene2DTransformSection* NewSection = CastChecked<UMovieScene2DTransformSection>(FindOrAddSection(Time));

		NewSection->AddKey(Time, TransformKey);

		return true;
	}
	return false;
}