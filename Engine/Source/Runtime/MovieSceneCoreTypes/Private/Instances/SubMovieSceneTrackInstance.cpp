// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "SubMovieSceneTrackInstance.h"
#include "MovieSceneInstance.h"
#include "SubMovieSceneSection.h"
#include "IMovieScenePlayer.h"


FSubMovieSceneTrackInstance::FSubMovieSceneTrackInstance( USubMovieSceneTrack& InTrack )
{
	SubMovieSceneTrack = &InTrack;
}

void FSubMovieSceneTrackInstance::RefreshInstance( const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player )
{
	TArray<UMovieSceneSection*> AllSections = SubMovieSceneTrack->GetAllSections();

	// Sections we encountered
	TSet<UMovieSceneSection*> FoundSections;

	// Iterate through each section and create an instance for it if it doesn't exist
	for( int32 SectionIndex = 0; SectionIndex < AllSections.Num(); ++SectionIndex )
	{
		USubMovieSceneSection* Section = CastChecked<USubMovieSceneSection>( AllSections[SectionIndex] );

		FoundSections.Add( Section );

		TSharedPtr<FMovieSceneInstance> Instance = SubMovieSceneInstances.FindRef(Section);
		if( !Instance.IsValid() )
		{
			Instance = MakeShareable( new FMovieSceneInstance( *Section->GetMovieScene() ) );

			Player.AddMovieSceneInstance( *Section, Instance.ToSharedRef() );

			SubMovieSceneInstances.Add( Section, Instance.ToSharedRef() );
		}

		// Refresh the existing instance
		Instance->RefreshInstance( Player );
	}

	TMap< TWeakObjectPtr<USubMovieSceneSection>, TSharedPtr<FMovieSceneInstance> >::TIterator It =  SubMovieSceneInstances.CreateIterator();
	for( ; It; ++It )
	{
		// Remove any sections that no longer exist
		if( !FoundSections.Contains( It.Key().Get() ) )
		{
			Player.RemoveMovieSceneInstance( *It.Key().Get(), It.Value().ToSharedRef() );
			It.RemoveCurrent();
		}
	}
}

void FSubMovieSceneTrackInstance::Update( float Position, float LastPosition, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player ) 
{
	TArray<UMovieSceneSection*> AllSections = SubMovieSceneTrack->GetAllSections();

	TArray<UMovieSceneSection*> TraversedSections = MovieSceneHelpers::GetTraversedSections( AllSections, Position, LastPosition );

	for( int32 SectionIndex = 0; SectionIndex < TraversedSections.Num(); ++SectionIndex )
	{
		USubMovieSceneSection* Section = CastChecked<USubMovieSceneSection>( TraversedSections[SectionIndex] );

		TSharedPtr<FMovieSceneInstance>& Instance = SubMovieSceneInstances.FindChecked( Section );

		UMovieScene* MovieScene = Section->GetMovieScene();

		// @todo Sequencer we should not do this every update
		TRange<float> TimeRange = MovieScene->GetTimeRange();

		// Position for the movie scene needs to be in local space
		float LocalDelta = TimeRange.GetLowerBoundValue() - Section->GetStartTime();
		float LocalPosition = Position + LocalDelta;


		Instance->Update( LocalPosition, LastPosition + LocalDelta, Player );
	}

}
