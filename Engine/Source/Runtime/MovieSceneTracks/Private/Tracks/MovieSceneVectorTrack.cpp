// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneVectorSection.h"
#include "MovieSceneVectorTrack.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneVectorTrackInstance.h"


UMovieSceneVectorTrack::UMovieSceneVectorTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	NumChannelsUsed = 0;
}


UMovieSceneSection* UMovieSceneVectorTrack::CreateNewSection()
{
	UMovieSceneVectorSection* NewSection = NewObject<UMovieSceneVectorSection>(this, UMovieSceneVectorSection::StaticClass());
	NewSection->SetChannelsUsed(NumChannelsUsed);
	return NewSection;
}


TSharedPtr<IMovieSceneTrackInstance> UMovieSceneVectorTrack::CreateInstance()
{
	return MakeShareable( new FMovieSceneVectorTrackInstance( *this ) );
}


bool UMovieSceneVectorTrack::Eval( float Position, float LastPosition, FVector4& InOutVector ) const
{
	const UMovieSceneSection* Section = MovieSceneHelpers::FindNearestSectionAtTime( Sections, Position );

	if( Section )
	{
		if (!Section->IsInfinite())
		{
			Position = FMath::Clamp(Position, Section->GetStartTime(), Section->GetEndTime());
		}

		InOutVector = CastChecked<UMovieSceneVectorSection>( Section )->Eval( Position, InOutVector );
	}

	return Section != nullptr;
}

