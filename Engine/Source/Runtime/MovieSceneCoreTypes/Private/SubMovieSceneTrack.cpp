// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "SubMovieSceneTrack.h"
#include "IMovieScenePlayer.h"
#include "SubMovieSceneSection.h"
#include "SubMovieSceneTrackInstance.h"

USubMovieSceneTrack::USubMovieSceneTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

UMovieSceneSection* USubMovieSceneTrack::CreateNewSection()
{
	return NewObject<USubMovieSceneSection>(this, NAME_None, RF_Transactional);
}

FName USubMovieSceneTrack::GetTrackName() const
{
	static const FName UniqueName( "Scenes" );

	return UniqueName;
}

TSharedPtr<IMovieSceneTrackInstance> USubMovieSceneTrack::CreateInstance()
{
	return MakeShareable( new FSubMovieSceneTrackInstance( *this ) ); 
}

TArray<UMovieSceneSection*> USubMovieSceneTrack::GetAllSections() const 
{
	TArray<UMovieSceneSection*> AllSections;
	for( int32 Section = 0; Section < SubMovieSceneSections.Num(); ++Section )
	{
		AllSections.Add( SubMovieSceneSections[Section] );
	}

	return AllSections;
}

void USubMovieSceneTrack::RemoveSection( UMovieSceneSection* Section )
{
	SubMovieSceneSections.Remove( Section );
}

bool USubMovieSceneTrack::IsEmpty() const
{
	return SubMovieSceneSections.Num() == 0;
}

TRange<float> USubMovieSceneTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;
	for (int32 SectionIndex = 0; SectionIndex < SubMovieSceneSections.Num(); ++SectionIndex)
	{
		Bounds.Add(SubMovieSceneSections[SectionIndex]->GetRange());
	}
	return TRange<float>::Hull(Bounds);
}

bool USubMovieSceneTrack::HasSection( UMovieSceneSection* Section ) const
{
	return SubMovieSceneSections.Contains( Section );
}

void USubMovieSceneTrack::AddMovieSceneSection( UMovieScene* SubMovieScene, float Time )
{
	Modify();

	USubMovieSceneSection* NewSection = CastChecked<USubMovieSceneSection>( CreateNewSection() );

	NewSection->SetMovieScene( SubMovieScene );
	NewSection->SetStartTime( Time );
	NewSection->SetEndTime( Time + SubMovieScene->GetTimeRange().Size<float>() );

	SubMovieSceneSections.Add( NewSection );

}
