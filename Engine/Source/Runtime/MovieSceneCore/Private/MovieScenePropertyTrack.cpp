// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCorePrivatePCH.h"
#include "MovieScenePropertyTrack.h"


UMovieScenePropertyTrack::UMovieScenePropertyTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	bSectionsAreShowable = false;
}


void UMovieScenePropertyTrack::SetPropertyNameAndPath( FName InPropertyName, const FString& InPropertyPath )
{
	check( InPropertyName != NAME_None && !InPropertyPath.IsEmpty() );

	PropertyName = InPropertyName;
	PropertyPath = InPropertyPath;
}

TArray<UMovieSceneSection*> UMovieScenePropertyTrack::GetAllSections() const
{
	TArray<UMovieSceneSection*> OutSectionPayloads;
	for( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
	{
		OutSectionPayloads.Add( Sections[SectionIndex] );
	}
	
	return OutSectionPayloads;
}


void UMovieScenePropertyTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}

bool UMovieScenePropertyTrack::HasSection( UMovieSceneSection* Section ) const 
{
	return Sections.Contains( Section );
}

void UMovieScenePropertyTrack::RemoveSection( UMovieSceneSection* Section ) 
{
	Sections.Remove( Section );
}

bool UMovieScenePropertyTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}



UMovieSceneSection* UMovieScenePropertyTrack::FindOrAddSection( float Time )
{
	// Find a spot for the section so that they are sorted by start time
	for( int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex )
	{
		UMovieSceneSection* Section = Sections[SectionIndex];

		if( Section->IsTimeWithinSection( Time ) )
		{
			return Section;
		}

		// Check if there are no more sections that would overlap the time 
		if( !Sections.IsValidIndex( SectionIndex+1 ) || Sections[SectionIndex+1]->GetStartTime() > Time )
		{
			// No sections overlap the time
		
			if( SectionIndex > 0 )
			{
				// Append and grow the previous section
				UMovieSceneSection* PreviousSection = Sections[ SectionIndex ? SectionIndex-1 : 0 ];
		
				PreviousSection->SetEndTime( Time );
				return PreviousSection;
			}
			else if( Sections.IsValidIndex( SectionIndex+1 ) )
			{
				// Prepend and grow the next section because there are no sections before this one
				UMovieSceneSection* NextSection = Sections[SectionIndex+1];
				NextSection->SetStartTime( Time );
				return NextSection;
			}	
			else
			{
				// SectionIndex == 0 
				UMovieSceneSection* PreviousSection = Sections[0];
				if( PreviousSection->GetEndTime() < Time )
				{
					// Append and grow the section
					PreviousSection->SetEndTime( Time );
				}
				else
				{
					// Prepend and grow the section
					PreviousSection->SetStartTime( Time );
				}
				return PreviousSection;
			}
		}

	}

	check( Sections.Num() == 0 );

	// Add a new section that starts and ends at the same time
	UMovieSceneSection* NewSection = CreateNewSection();
	NewSection->SetStartTime( Time );
	NewSection->SetEndTime( Time );

	Sections.Add( NewSection );

	return NewSection;
}

TRange<float> UMovieScenePropertyTrack::GetSectionBoundaries() const
{
	TArray< TRange<float> > Bounds;
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		Bounds.Add(Sections[SectionIndex]->GetRange());
	}
	return TRange<float>::Hull(Bounds);
}
