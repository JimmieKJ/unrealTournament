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
		if (FPackageName::IsShortPackageName(SafeLevelName))
		{
			// Make sure MyMap1 and Map1 names do not resolve to a same streaming level
			SafeLevelName = TEXT("/") + SafeLevelName;
		}

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

bool GetLevelVisibility( ULevelStreaming* Level )
{
#if WITH_EDITOR
	if ( GIsEditor && Level->GetWorld()->IsPlayInEditor() == false )
	{
		return Level->bShouldBeVisibleInEditor;
	}
	else
#endif
	{
		return Level->bShouldBeVisible;
	}
}

void SetLevelVisibility( ULevelStreaming* Level, bool bVisible )
{
#if WITH_EDITOR
	if ( GIsEditor && Level->GetWorld()->IsPlayInEditor() == false )
	{
		Level->bShouldBeVisibleInEditor = bVisible;
		Level->GetWorld()->FlushLevelStreaming();

		// Iterate over the level's actors
		TArray<AActor*>& Actors = Level->GetLoadedLevel()->Actors;
		for ( int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex )
		{
			AActor* Actor = Actors[ActorIndex];
			if ( Actor )
			{
				if (Actor->bHiddenEdLevel == bVisible )
				{
					Actor->bHiddenEdLevel = !bVisible;
					if ( bVisible )
					{
						Actor->ReregisterAllComponents();
					}
					else
					{
						Actor->UnregisterAllComponents();
					}
				}
			}
		}
	}
	else
#endif
	{
		Level->bShouldBeVisible = bVisible;
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
			SetLevelVisibility( StreamingLevel, *SavedVisibility );
		}
	}

	SavedLevelVisibility.Empty();
}


void FMovieSceneLevelVisibilityTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<TWeakObjectPtr<UObject>>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance ) 
{
	bool bVisibilityModified = false;
	UWorld* CurrentWorld = nullptr;

	TArray<UMovieSceneLevelVisibilitySection*> CurrentActiveSections;
	if ( UpdateData.bSubSceneDeactivate == false )
	{
		for ( UMovieSceneSection* Section : LevelVisibilityTrack->GetAllSections() )
		{
			UMovieSceneLevelVisibilitySection* LevelVisibilitySection = Cast<UMovieSceneLevelVisibilitySection>( Section );
			if ( LevelVisibilitySection != nullptr &&
				LevelVisibilitySection->GetStartTime() <= UpdateData.Position &&
				LevelVisibilitySection->GetEndTime() > UpdateData.Position )
			{
				CurrentActiveSections.Add( LevelVisibilitySection );
			}
		}
	}

	TArray<UMovieSceneLevelVisibilitySection*> NewActiveSections = CurrentActiveSections;
	for (int i = ActiveSectionDatas.Num() - 1; i >= 0; i--)
	{
		FActiveSectionData& ActiveSectionData = ActiveSectionDatas[i];
		if ( ActiveSectionData.Section.IsValid() )
		{
			if ( CurrentActiveSections.Contains( ActiveSectionData.Section ) )
			{
				NewActiveSections.Remove( ActiveSectionData.Section.Get() );
			}
			else
			{
				for ( auto LevelNamePreviousVisibilityPair : ActiveSectionData.PreviousLevelVisibility )
				{
					FName SafeLevelName( *MakeSafeLevelName( LevelNamePreviousVisibilityPair.Key ) );
					TWeakObjectPtr<ULevelStreaming>* FoundStreamingLevel = NameToLevelMap.Find( SafeLevelName );
					if ( FoundStreamingLevel != nullptr && FoundStreamingLevel->IsValid() )
					{
						if ( GetLevelVisibility( FoundStreamingLevel->Get() ) != LevelNamePreviousVisibilityPair.Value )
						{
							SetLevelVisibility( FoundStreamingLevel->Get(), LevelNamePreviousVisibilityPair.Value );
							bVisibilityModified = true;
							CurrentWorld = (*FoundStreamingLevel)->GetWorld();
						}
					}
				}
				ActiveSectionDatas.RemoveAt( i );
			}
		}
		else
		{
			ActiveSectionDatas.RemoveAt( i );
		}
	}

	for ( UMovieSceneLevelVisibilitySection* LevelVisibilitySection : NewActiveSections )
	{
		FActiveSectionData ActiveSectionData;
		ActiveSectionData.Section = LevelVisibilitySection;
		bool bShouldBeVisible = LevelVisibilitySection->GetVisibility() == ELevelVisibility::Visible ? true : false;
		

		for ( FName LevelName : *LevelVisibilitySection->GetLevelNames() )
		{
			FName SafeLevelName ( *MakeSafeLevelName( LevelName ) );
			ULevelStreaming* StreamingLevel = nullptr;
			TWeakObjectPtr<ULevelStreaming>* FoundStreamingLevel = NameToLevelMap.Find( SafeLevelName );

			if ( FoundStreamingLevel != nullptr )
			{
				StreamingLevel = FoundStreamingLevel->Get();
			}
			else if ( GIsEditor )
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
				ActiveSectionData.PreviousLevelVisibility.Add(LevelName, GetLevelVisibility( StreamingLevel ) );
				if ( GetLevelVisibility( StreamingLevel ) != bShouldBeVisible )
				{
					SetLevelVisibility( StreamingLevel, bShouldBeVisible );
					bVisibilityModified = true;
					CurrentWorld = StreamingLevel->GetWorld();
				}
			}
		}
		ActiveSectionDatas.Emplace(ActiveSectionData);
	}

	if ( bVisibilityModified && CurrentWorld != nullptr )
	{
		CurrentWorld->FlushLevelStreaming( EFlushLevelStreamingType::Visibility );
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
	SavedLevelVisibility.Add( FObjectKey( StreamingLevel ), GetLevelVisibility(StreamingLevel) );
}
