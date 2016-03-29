// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneLevelVisibilityTrack.h"
#include "MovieSceneLevelVisibilityTrackInstance.h"


FMovieSceneLevelVisibilityTrackInstance::FMovieSceneLevelVisibilityTrackInstance( UMovieSceneLevelVisibilityTrack& InLevelVisibilityTrack )
{
	LevelVisibilityTrack = &InLevelVisibilityTrack;
}

// TODO: This was copied from LevelStreaming.cpp, it should be in a set of shared utilities somewhere.
FString MakeSafeLevelName( const FName& InLevelName )
{
	// Special case for PIE, the PackageName gets mangled.
	if ( !GWorld->StreamingLevelsPrefix.IsEmpty() )
	{
		FString PackageName = GWorld->StreamingLevelsPrefix + FPackageName::GetShortName( InLevelName );
		if ( !FPackageName::IsShortPackageName( InLevelName ) )
		{
			PackageName = FPackageName::GetLongPackagePath( InLevelName.ToString() ) + TEXT( "/" ) + PackageName;
		}

		return PackageName;
	}

	return InLevelName.ToString();
}


ULevelStreaming* GetStreamingLevel( FName LevelName )
{
	if ( LevelName != NAME_None )
	{
		FString SafeLevelName = MakeSafeLevelName( LevelName );
		for ( ULevelStreaming* LevelStreaming : GWorld->StreamingLevels )
		{
			if ( LevelStreaming && LevelStreaming->GetWorldAssetPackageName().EndsWith( SafeLevelName, ESearchCase::IgnoreCase ) )
			{
				return LevelStreaming;
			}
		}
	}

	return nullptr;
}


void GetAnimatedStreamingLevels( UMovieSceneLevelVisibilityTrack* LevelVisibilityTrack, TArray<ULevelStreaming*>& StreamingLevels )
{
	if ( LevelVisibilityTrack != nullptr )
	{
		for ( UMovieSceneSection* Section : LevelVisibilityTrack->GetAllSections() )
		{
			UMovieSceneLevelVisibilitySection* LevelVisibilitySection = Cast<UMovieSceneLevelVisibilitySection>( Section );
			if ( LevelVisibilitySection != nullptr )
			{
				for ( FName StreamingLevelName : *LevelVisibilitySection->GetLevelNames() )
				{
					ULevelStreaming* StreamingLevel = GetStreamingLevel( StreamingLevelName );
					if ( StreamingLevel != nullptr )
					{
						StreamingLevels.Add( StreamingLevel );
					}
				}
			}
		}
	}
}


void FMovieSceneLevelVisibilityTrackInstance::SaveState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	TArray<ULevelStreaming*> AnimatedStreamingLevels;
	GetAnimatedStreamingLevels(LevelVisibilityTrack, AnimatedStreamingLevels);

	for ( ULevelStreaming* StreamingLevel : AnimatedStreamingLevels )
	{
		SaveLevelVisibilityState( StreamingLevel );
	}
}


void FMovieSceneLevelVisibilityTrackInstance::RestoreState(const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	TArray<ULevelStreaming*> AnimatedStreamingLevels;
	GetAnimatedStreamingLevels( LevelVisibilityTrack, AnimatedStreamingLevels );

	for ( ULevelStreaming* StreamingLevel : AnimatedStreamingLevels )
	{
		bool* SavedVisibility = SavedLevelVisibility.Find( FObjectKey( StreamingLevel ) );
		if ( SavedVisibility != nullptr )
		{
			SetEditorLevelVisibilityState( StreamingLevel, *SavedVisibility );
		}
	}

	SavedLevelVisibility.Empty();
}


void FMovieSceneLevelVisibilityTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) 
{
	for ( UMovieSceneSection* Section : LevelVisibilityTrack->GetAllSections() )
	{
		UMovieSceneLevelVisibilitySection* LevelVisibilitySection = Cast<UMovieSceneLevelVisibilitySection>(Section);
		if (LevelVisibilitySection != nullptr &&
			LevelVisibilitySection->GetStartTime() <= UpdateData.Position &&
			LevelVisibilitySection->GetEndTime() > UpdateData.Position )
		{
			bool bShouldBeVisible = LevelVisibilitySection->GetVisibility() == ELevelVisibility::Visible ? true : false;
			bool bVisibilityModified = false;
			for ( FName LevelName : *LevelVisibilitySection->GetLevelNames() )
			{
				FName SafeLevelName ( *MakeSafeLevelName( LevelName ) );
				ULevelStreaming* StreamingLevel = nullptr;
				TWeakObjectPtr<ULevelStreaming>* FoundStreamingLevel = NameToLevelMap.Find( SafeLevelName );

				if ( FoundStreamingLevel != nullptr )
				{
					StreamingLevel = FoundStreamingLevel->Get();
				}
				else if ( GIsEditor && !GWorld->HasBegunPlay() )
				{
					// If we're in the editor, it possible the level list has been updated since the last refresh, so try to find the level.
					StreamingLevel = GetStreamingLevel( LevelName );
					if ( StreamingLevel != nullptr )
					{
						SaveLevelVisibilityState( StreamingLevel );
						NameToLevelMap.Add( LevelName, TWeakObjectPtr<ULevelStreaming>( StreamingLevel ) );
					}
				}

				if ( StreamingLevel != nullptr )
				{
					if ( GIsEditor && !GWorld->HasBegunPlay() )
					{
						SetEditorLevelVisibilityState( StreamingLevel, bShouldBeVisible );
					}
					else
					{
						if ( StreamingLevel->bShouldBeVisible != bShouldBeVisible )
						{
							StreamingLevel->bShouldBeVisible = bShouldBeVisible;
							bVisibilityModified = true;
						}
					}
					
				}
			}
			if ( bVisibilityModified )
			{
				GWorld->FlushLevelStreaming( EFlushLevelStreamingType::Visibility );
			}
		}
	}
}


void FMovieSceneLevelVisibilityTrackInstance::RefreshInstance( const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance )
{
	NameToLevelMap.Empty();

	TArray<ULevelStreaming*> AnimatedStreamingLevels;
	GetAnimatedStreamingLevels( LevelVisibilityTrack, AnimatedStreamingLevels );

	for ( ULevelStreaming* StreamingLevel : AnimatedStreamingLevels )
	{
		NameToLevelMap.Add( FPackageName::GetShortFName( StreamingLevel->GetWorldAssetPackageFName() ), TWeakObjectPtr<ULevelStreaming>( StreamingLevel ) );
	}
}

void FMovieSceneLevelVisibilityTrackInstance::SaveLevelVisibilityState( ULevelStreaming* StreamingLevel )
{
	SavedLevelVisibility.Add( FObjectKey( StreamingLevel ), StreamingLevel->bShouldBeVisibleInEditor );
}

void FMovieSceneLevelVisibilityTrackInstance::SetEditorLevelVisibilityState( ULevelStreaming* StreamingLevel, bool bShouldBeVisible )
{
	if ( StreamingLevel->bShouldBeVisibleInEditor != bShouldBeVisible )
	{
		StreamingLevel->bShouldBeVisibleInEditor = bShouldBeVisible;
		ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
		if ( LoadedLevel != nullptr )
		{
			for ( AActor* Actor : LoadedLevel->Actors )
			{
				if ( Actor != nullptr )
				{
#if WITH_EDITOR
					Actor->SetIsTemporarilyHiddenInEditor( !bShouldBeVisible );
#endif
				}
			}
		}
	}
}
