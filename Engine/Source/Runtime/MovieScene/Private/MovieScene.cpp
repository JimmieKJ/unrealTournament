// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieScenePrivatePCH.h"
#include "MovieScene.h"


/* UMovieScene interface
 *****************************************************************************/

UMovieScene::UMovieScene(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PlaybackRange(FFloatRange::Empty())
	, InTime_DEPRECATED(FLT_MAX)
	, OutTime_DEPRECATED(-FLT_MAX)
	, StartTime_DEPRECATED(FLT_MAX)
	, EndTime_DEPRECATED(-FLT_MAX)
{
#if WITH_EDITORONLY_DATA
	EditorData.WorkingRange = EditorData.ViewRange = TRange<float>::Empty();
#endif
}


#if WITH_EDITOR

// @todo sequencer: Some of these methods should only be used by tools, and should probably move out of MovieScene!
FGuid UMovieScene::AddSpawnable( const FString& Name, UBlueprint* Blueprint )
{
	check( (Blueprint != nullptr) && (Blueprint->GeneratedClass) );

	Modify();

	FMovieSceneSpawnable NewSpawnable( Name, Blueprint->GeneratedClass );
	Spawnables.Add( NewSpawnable );

	// Add a new binding so that tracks can be added to it
	new (ObjectBindings) FMovieSceneBinding( NewSpawnable.GetGuid(), NewSpawnable.GetName() );

	return NewSpawnable.GetGuid();
}


bool UMovieScene::RemoveSpawnable( const FGuid& Guid )
{
	bool bAnythingRemoved = false;
	if( ensure( Guid.IsValid() ) )
	{
		for( auto SpawnableIter( Spawnables.CreateIterator() ); SpawnableIter; ++SpawnableIter )
		{
			auto& CurSpawnable = *SpawnableIter;
			if( CurSpawnable.GetGuid() == Guid )
			{
				Modify();

				{
					UClass* GeneratedClass = CurSpawnable.GetClass();
					UBlueprint* Blueprint = GeneratedClass ? Cast<UBlueprint>(GeneratedClass->ClassGeneratedBy) : nullptr;
					check(nullptr != Blueprint);
					// @todo sequencer: Also remove created Blueprint inner object.  Is this sufficient?  Needs to work with Undo too!
					Blueprint->ClearFlags( RF_Standalone );	// @todo sequencer: Probably not needed for Blueprint
					Blueprint->MarkPendingKill();
				}

				RemoveBinding( Guid );

				// Found it!
				Spawnables.RemoveAt( SpawnableIter.GetIndex() );


				bAnythingRemoved = true;
				break;
			}
		}
	}

	return bAnythingRemoved;
}
#endif //WITH_EDITOR


int32 UMovieScene::GetSpawnableCount() const
{
	return Spawnables.Num();
}

FMovieSceneSpawnable* UMovieScene::FindSpawnable( const FGuid& Guid )
{
	return Spawnables.FindByPredicate([&](FMovieSceneSpawnable& Spawnable) {
		return Spawnable.GetGuid() == Guid;
	});
}


FGuid UMovieScene::AddPossessable( const FString& Name, UClass* Class )
{
	Modify();

	FMovieScenePossessable NewPossessable( Name, Class );
	Possessables.Add( NewPossessable );

	// Add a new binding so that tracks can be added to it
	new (ObjectBindings) FMovieSceneBinding( NewPossessable.GetGuid(), NewPossessable.GetName() );

	return NewPossessable.GetGuid();
}


bool UMovieScene::RemovePossessable( const FGuid& PossessableGuid )
{
	bool bAnythingRemoved = false;

	for( auto PossesableIter( Possessables.CreateIterator() ); PossesableIter; ++PossesableIter )
	{
		auto& CurPossesable = *PossesableIter;

		if( CurPossesable.GetGuid() == PossessableGuid )
		{	
			Modify();

			// Remove the parent-child link for a parent spawnable/child possessable if necessary
			if (CurPossesable.GetParent().IsValid())
			{
				FMovieSceneSpawnable* ParentSpawnable = FindSpawnable(CurPossesable.GetParent());
				if (ParentSpawnable)
				{
					ParentSpawnable->RemoveChildPossessable(PossessableGuid);
				}
			}

			// Found it!
			Possessables.RemoveAt( PossesableIter.GetIndex() );

			RemoveBinding( PossessableGuid );

			bAnythingRemoved = true;
			break;
		}
	}

	return bAnythingRemoved;
}


bool UMovieScene::ReplacePossessable( const FGuid& OldGuid, const FGuid& NewGuid, const FString& NewName )
{
	bool bAnythingReplaced = false;

	for (auto& Possessable : Possessables)
	{
		if (Possessable.GetGuid() == OldGuid)
		{	
			Modify();

			// Found it!
			Possessable.SetGuid(NewGuid);
			Possessable.SetName(NewName);

			ReplaceBinding( OldGuid, NewGuid, NewName );
			bAnythingReplaced = true;

			break;
		}
	}

	return bAnythingReplaced;
}


FMovieScenePossessable* UMovieScene::FindPossessable( const FGuid& Guid )
{
	for (auto& Possessable : Possessables)
	{
		if (Possessable.GetGuid() == Guid)
		{
			return &Possessable;
		}
	}

	return nullptr;
}


int32 UMovieScene::GetPossessableCount() const
{
	return Possessables.Num();
}


FMovieScenePossessable& UMovieScene::GetPossessable( const int32 Index )
{
	return Possessables[Index];
}


FText UMovieScene::GetObjectDisplayName(const FGuid& ObjectId)
{
#if WITH_EDITORONLY_DATA
	FText& Result = ObjectsToDisplayNames.FindOrAdd(ObjectId.ToString());

	if (!Result.IsEmpty())
	{
		return Result;
	}

	FMovieSceneSpawnable* Spawnable = FindSpawnable(ObjectId);

	if (Spawnable != nullptr)
		{
		Result = FText::FromString(Spawnable->GetName());
		return Result;
		}

	FMovieScenePossessable* Possessable = FindPossessable(ObjectId);

	if (Possessable != nullptr)
		{
		Result = FText::FromString(Possessable->GetName());
		return Result;
	}
#endif
	return FText::GetEmpty();
}


#if WITH_EDITORONLY_DATA
int32 UMovieScene::GetAllLabels(TArray<FString>& OutLabels) const
{
	for (const auto& LabelsPair : ObjectsToLabels)
	{
		for (const auto& Label : LabelsPair.Value.Strings)
		{
			OutLabels.AddUnique(Label);
		}
	}

	return OutLabels.Num();
}


bool UMovieScene::LabelExists(const FString& Label) const
{
	for (const auto& LabelsPair : ObjectsToLabels)
	{
		if (LabelsPair.Value.Strings.Contains(Label))
		{
			return true;
		}
	}

	return false;
}
#endif


TRange<float> UMovieScene::GetPlaybackRange() const
{
	check(PlaybackRange.HasLowerBound() && PlaybackRange.HasUpperBound());
	return PlaybackRange;
}


void UMovieScene::SetPlaybackRange(float Start, float End)
{
	if (ensure(End >= Start))
	{
		Modify();
		
		PlaybackRange = TRange<float>(Start, TRangeBound<float>::Inclusive(End));

#if WITH_EDITORONLY_DATA
		if (EditorData.WorkingRange.IsEmpty())
		{
			EditorData.WorkingRange = PlaybackRange;
		}
		if (EditorData.ViewRange.IsEmpty())
		{
			EditorData.ViewRange = PlaybackRange;
		}
#endif
	}
}

TArray<UMovieSceneSection*> UMovieScene::GetAllSections() const
{
	TArray<UMovieSceneSection*> OutSections;

	// Add all master type sections 
	for( int32 TrackIndex = 0; TrackIndex < MasterTracks.Num(); ++TrackIndex )
	{
		OutSections.Append( MasterTracks[TrackIndex]->GetAllSections() );
	}

	// Add all object binding sections
	for (const auto& Binding : ObjectBindings)
	{
		for (const auto& Track : Binding.GetTracks())
		{
			OutSections.Append(Track->GetAllSections());
		}
	}

	return OutSections;
}


UMovieSceneTrack* UMovieScene::FindTrack(TSubclassOf<UMovieSceneTrack> TrackClass, const FGuid& ObjectGuid, const FName& TrackName) const
{
	check( ObjectGuid.IsValid() );
	
	for (const auto& Binding : ObjectBindings)
	{
		if (Binding.GetObjectGuid() != ObjectGuid) 
	{
			continue;
		}

		for (const auto& Track : Binding.GetTracks())
		{
			if ((Track->GetClass() == TrackClass) && (Track->GetTrackName() == TrackName))
			{
				return Track;
			}
		}
	}
	
	return nullptr;
}


class UMovieSceneTrack* UMovieScene::AddTrack( TSubclassOf<UMovieSceneTrack> TrackClass, const FGuid& ObjectGuid )
{
	UMovieSceneTrack* CreatedType = nullptr;

	check( ObjectGuid.IsValid() )

	for (auto& Binding : ObjectBindings)
	{
		if( Binding.GetObjectGuid() == ObjectGuid ) 
		{
			Modify();

			CreatedType = NewObject<UMovieSceneTrack>(this, TrackClass, NAME_None, RF_Transactional);
			ensure(CreatedType);

			Binding.AddTrack( *CreatedType );
		}
	}

	return CreatedType;
}


bool UMovieScene::RemoveTrack(UMovieSceneTrack& Track)
{
	Modify();

	bool bAnythingRemoved = false;

	for (auto& Binding : ObjectBindings)
	{
		if (Binding.RemoveTrack(Track))
		{
			bAnythingRemoved = true;

			// The track was removed from the current binding, stop
			// searching now as it cannot exist in any other binding
			break;
		}
	}

	return bAnythingRemoved;
}


UMovieSceneTrack* UMovieScene::FindMasterTrack( TSubclassOf<UMovieSceneTrack> TrackClass ) const
{
	UMovieSceneTrack* FoundTrack = nullptr;

	for (const auto Track : MasterTracks)
	{
		if( Track->GetClass() == TrackClass )
		{
			FoundTrack = Track;
			break;
		}
	}

	return FoundTrack;
}


class UMovieSceneTrack* UMovieScene::AddMasterTrack( TSubclassOf<UMovieSceneTrack> TrackClass )
{
	Modify();

	UMovieSceneTrack* CreatedType = NewObject<UMovieSceneTrack>(this, TrackClass, NAME_None, RF_Transactional);
	MasterTracks.Add( CreatedType );
	
	return CreatedType;
}


UMovieSceneTrack* UMovieScene::AddShotTrack( TSubclassOf<UMovieSceneTrack> TrackClass )
{
	if( !ShotTrack )
	{
		Modify();
		ShotTrack = NewObject<UMovieSceneTrack>(this, TrackClass, FName("Shots"), RF_Transactional);
	}

	return ShotTrack;
}


UMovieSceneTrack* UMovieScene::GetShotTrack()
{
	return ShotTrack;
}


void UMovieScene::RemoveShotTrack()
{
	if( ShotTrack )
	{
		Modify();
		ShotTrack = nullptr;
	}
}


bool UMovieScene::RemoveMasterTrack(UMovieSceneTrack& Track) 
{
	Modify();

	return (MasterTracks.RemoveSingle(&Track) != 0);
}


bool UMovieScene::IsAMasterTrack(const UMovieSceneTrack& Track) const
{
	for ( const UMovieSceneTrack* MasterTrack : MasterTracks)
	{
		if (&Track == MasterTrack)
		{
			return true;
		}
	}

	return false;
}


void UMovieScene::UpgradeTimeRanges()
{
	// Legacy upgrade for playback ranges:
	// We used to optionally store a start/end and in/out time for sequences.
	// The only 2 uses were UWidgetAnimations and ULevelSequences.
	// Widget animations used to always calculate their length automatically, from the section boundaries, and always started at 0
	// Level sequences defaulted to having a fixed play range.
	// We now expose the playback range more visibly, but we need to upgrade the old data.

	if (InTime_DEPRECATED != FLT_MAX && OutTime_DEPRECATED != -FLT_MAX)
	{
		// Finite range already defined in old data
		PlaybackRange = TRange<float>(InTime_DEPRECATED, TRangeBound<float>::Inclusive(OutTime_DEPRECATED));
	}
	else if (PlaybackRange.IsEmpty())
	{
		// No range specified, so automatically calculate one by determining the maximum upper bound of the sequence
		// In this instance (UMG), playback always started at 0
		float MaxBound = 0.f;

		for (const auto& Track : MasterTracks)
		{
			auto Range = Track->GetSectionBoundaries();
			if (Range.HasUpperBound())
			{
				MaxBound = FMath::Max(MaxBound, Range.GetUpperBoundValue());
			}
		}

		for (const auto& Binding : ObjectBindings)
		{
			auto Range = Binding.GetTimeRange();
			if (Range.HasUpperBound())
			{
				MaxBound = FMath::Max(MaxBound, Range.GetUpperBoundValue());
			}
		}

		PlaybackRange = TRange<float>(0.f, TRangeBound<float>::Inclusive(MaxBound));
	}
	else if (PlaybackRange.GetUpperBound().IsExclusive())
	{
		// playback ranges are now always inclusive
		PlaybackRange = TRange<float>(PlaybackRange.GetLowerBound(), TRangeBound<float>::Inclusive(PlaybackRange.GetUpperBoundValue()));
	}

	// PlaybackRange must always be defined to a finite range
	if (!PlaybackRange.HasLowerBound() || !PlaybackRange.HasUpperBound() || PlaybackRange.IsDegenerate())
	{
		PlaybackRange = TRange<float>(0.f, 0.f);
	}

#if WITH_EDITORONLY_DATA
	// Legacy upgrade for working range
	if (StartTime_DEPRECATED != FLT_MAX && EndTime_DEPRECATED != -FLT_MAX)
	{
		EditorData.WorkingRange = TRange<float>(StartTime_DEPRECATED, EndTime_DEPRECATED);
	}
	else if (EditorData.WorkingRange.IsEmpty())
	{
		EditorData.WorkingRange = PlaybackRange;
	}

	if (EditorData.ViewRange.IsEmpty())
	{
		EditorData.ViewRange = PlaybackRange;
	}
#endif
}


/* UObject interface
 *****************************************************************************/

void UMovieScene::PostLoad()
{
	UpgradeTimeRanges();
	Super::PostLoad();
}


void UMovieScene::PreSave()
{
	Super::PreSave();

#if WITH_EDITORONLY_DATA
	// compress meta data mappings prior to saving
	for (auto It = ObjectsToDisplayNames.CreateIterator(); It; ++It)
	{
		FGuid ObjectId;

		if (!FGuid::Parse(It.Key(), ObjectId) || ((FindPossessable(ObjectId) == nullptr) && (FindSpawnable(ObjectId) == nullptr)))
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = ObjectsToLabels.CreateIterator(); It; ++It)
	{
		FGuid ObjectId;

		if (!FGuid::Parse(It.Key(), ObjectId) || ((FindPossessable(ObjectId) == nullptr) && (FindSpawnable(ObjectId) == nullptr)))
		{
			It.RemoveCurrent();
		}
	}
#endif
}


/* UMovieScene implementation
 *****************************************************************************/

void UMovieScene::RemoveBinding(const FGuid& Guid)
{
	// update each type
	for (int32 BindingIndex = 0; BindingIndex < ObjectBindings.Num(); ++BindingIndex)
	{
		if (ObjectBindings[BindingIndex].GetObjectGuid() == Guid)
		{
			ObjectBindings.RemoveAt(BindingIndex);
			break;
		}
	}
}


void UMovieScene::ReplaceBinding(const FGuid& OldGuid, const FGuid& NewGuid, const FString& Name)
{
	for (auto& Binding : ObjectBindings)
	{
		if (Binding.GetObjectGuid() == OldGuid)
		{
			Binding.SetObjectGuid(NewGuid);
			Binding.SetName(Name);
			break;
		}
	}
}