// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimMontage.cpp: Montage classes that contains slots
=============================================================================*/ 

#include "EnginePrivate.h"
#include "Animation/AnimMontage.h"
#include "AnimationUtils.h"
#include "AnimationRuntime.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequenceBase.h"

///////////////////////////////////////////////////////////////////////////
//
UAnimMontage::UAnimMontage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAnimBranchingPointNeedsSort = true;
	BlendInTime = 0.25f;
	BlendOutTime = 0.25f;
	BlendOutTriggerTime = -1.f;
}

bool UAnimMontage::IsValidSlot(FName InSlotName) const
{
	for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
	{
		if ( SlotAnimTracks[I].SlotName == InSlotName )
		{
			// if data is there, return true. Otherwise, it doesn't matter
			return ( SlotAnimTracks[I].AnimTrack.AnimSegments.Num() >  0 );
		}
	}

	return false;
}

const FAnimTrack* UAnimMontage::GetAnimationData(FName InSlotName) const
{
	for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
	{
		if ( SlotAnimTracks[I].SlotName == InSlotName )
		{
			// if data is there, return true. Otherwise, it doesn't matter
			return &( SlotAnimTracks[I].AnimTrack );
		}
	}

	return NULL;
}

#if WITH_EDITOR
/*
void UAnimMontage::SetAnimationMontage(FName SectionName, FName SlotName, UAnimSequence * Sequence)
{
	// if valid section
	if ( IsValidSectionName(SectionName) )
	{
		bool bNeedToAddSlot=true;
		bool bNeedToAddSection=true;
		// see if we need to replace current one first
		for ( int32 I=0; I<AnimMontages.Num() ; ++I )
		{
			if (AnimMontages(I).SectionName == SectionName)
			{
				// found the section, now find slot name
				for ( int32 J=0; J<AnimMontages(I).Animations.Num(); ++J )
				{
					if (AnimMontages(I).Animations(J).SlotName == SlotName)
					{
						AnimMontages(I).Animations(J).SlotAnim = Sequence;
						bNeedToAddSlot = false;
						break;
					}
				}

				// we didn't find any slot that works, break it
				if (bNeedToAddSlot)
				{
					AnimMontages(I).Animations.Add(FSlotAnimTracks(SlotName, Sequence));
				}

				// we found section, no need to add section
				bNeedToAddSection = false;
				break;
			}
		}

		if (bNeedToAddSection)
		{
			int32 NewIndex = AnimMontages.Add(FMontageSlot(SectionName));
			AnimMontages(NewIndex).Animations.Add(FSlotAnimTracks(SlotName, Sequence));
		}

		// rebuild section data to sync runtime data
		BuildSectionData();
	}	
}*/
#endif

bool UAnimMontage::IsWithinPos(int32 FirstIndex, int32 SecondIndex, float CurrentTime) const
{
	float StartTime;
	float EndTime;
	if ( CompositeSections.IsValidIndex(FirstIndex) )
	{
		StartTime = CompositeSections[FirstIndex].GetTime();
	}
	else // if first index isn't valid, set to be 0.f, so it starts from reset
	{
		StartTime = 0.f;
	}

	if ( CompositeSections.IsValidIndex(SecondIndex) )
	{
		EndTime = CompositeSections[SecondIndex].GetTime();
	}
	else // if end index isn't valid, set to be BIG_NUMBER
	{
		// @todo anim, I don't know if using SequenceLength is better or BIG_NUMBER
		// I don't think that'd matter. 
		EndTime = SequenceLength;
	}

	return (StartTime <= CurrentTime && EndTime > CurrentTime);
}

float UAnimMontage::CalculatePos(FCompositeSection &Section, float PosWithinCompositeSection) const
{
	float Offset = Section.GetTime();
	Offset += PosWithinCompositeSection;
	// @todo anim
	return Offset;
}

int32 UAnimMontage::GetSectionIndexFromPosition(float Position) const
{
	for (int32 I=0; I<CompositeSections.Num(); ++I)
	{
		// if within
		if( IsWithinPos(I, I+1, Position) )
		{
			return I;
		}
	}

	return INDEX_NONE;
}

int32 UAnimMontage::GetAnimCompositeSectionIndexFromPos(float CurrentTime, float& PosWithinCompositeSection) const
{
	PosWithinCompositeSection = 0.f;

	for (int32 I=0; I<CompositeSections.Num(); ++I)
	{
		// if within
		if (IsWithinPos(I, I+1, CurrentTime))
		{
			PosWithinCompositeSection = CurrentTime - CompositeSections[I].GetTime();
			return I;
		}
	}

	return INDEX_NONE;
}

float UAnimMontage::GetSectionTimeLeftFromPos(float Position)
{
	const int32 SectionID = GetSectionIndexFromPosition(Position);
	if( SectionID != INDEX_NONE )
	{
		if( IsValidSectionIndex(SectionID+1) )
		{
			return (GetAnimCompositeSection(SectionID+1).GetTime() - Position);
		}
		else
		{
			return (SequenceLength - Position);
		}
	}

	return -1.f;
}

const FCompositeSection& UAnimMontage::GetAnimCompositeSection(int32 SectionIndex) const
{
	check ( CompositeSections.IsValidIndex(SectionIndex) );
	return CompositeSections[SectionIndex];
}

FCompositeSection& UAnimMontage::GetAnimCompositeSection(int32 SectionIndex)
{
	check ( CompositeSections.IsValidIndex(SectionIndex) );
	return CompositeSections[SectionIndex];
}

int32 UAnimMontage::GetSectionIndex(FName InSectionName) const
{
	// I can have operator== to check SectionName, but then I have to construct
	// empty FCompositeSection all the time whenever I search :(
	for (int32 I=0; I<CompositeSections.Num(); ++I)
	{
		if ( CompositeSections[I].SectionName == InSectionName ) 
		{
			return I;
		}
	}

	return INDEX_NONE;
}

FName UAnimMontage::GetSectionName(int32 SectionIndex) const
{
	if ( CompositeSections.IsValidIndex(SectionIndex) )
	{
		return CompositeSections[SectionIndex].SectionName;
	}

	return NAME_None;
}

bool UAnimMontage::IsValidSectionName(FName InSectionName) const
{
	return GetSectionIndex(InSectionName) != INDEX_NONE;
}

bool UAnimMontage::IsValidSectionIndex(int32 SectionIndex) const
{
	return (CompositeSections.IsValidIndex(SectionIndex));
}

void UAnimMontage::GetSectionStartAndEndTime(int32 SectionIndex, float& OutStartTime, float& OutEndTime) const
{
	OutStartTime = 0.f;
	OutEndTime = SequenceLength;
	if ( IsValidSectionIndex(SectionIndex) )
	{
		OutStartTime = GetAnimCompositeSection(SectionIndex).GetTime();		
	}

	if ( IsValidSectionIndex(SectionIndex + 1))
	{
		OutEndTime = GetAnimCompositeSection(SectionIndex + 1).GetTime();		
	}
}

float UAnimMontage::GetSectionLength(int32 SectionIndex) const
{
	float StartTime = 0.f;
	float EndTime = SequenceLength;
	if ( IsValidSectionIndex(SectionIndex) )
	{
		StartTime = GetAnimCompositeSection(SectionIndex).GetTime();		
	}

	if ( IsValidSectionIndex(SectionIndex + 1))
	{
		EndTime = GetAnimCompositeSection(SectionIndex + 1).GetTime();		
	}

	return EndTime - StartTime;
}

#if WITH_EDITOR
int32 UAnimMontage::AddAnimCompositeSection(FName InSectionName, float StartTime)
{
	FCompositeSection NewSection;

	// make sure same name doesn't exists
	if ( InSectionName != NAME_None )
	{
		NewSection.SectionName = InSectionName;
	}
	else
	{
		// just give default name
		NewSection.SectionName = FName(*FString::Printf(TEXT("Section%d"), CompositeSections.Num()+1));
	}

	// we already have that name
	if ( GetSectionIndex(InSectionName)!=INDEX_NONE )
	{
		UE_LOG(LogAnimation, Warning, TEXT("AnimCompositeSection : %s(%s) already exists. Choose different name."), 
			*NewSection.SectionName.ToString(), *InSectionName.ToString());
		return INDEX_NONE;
	}

	NewSection.LinkMontage(this, StartTime);

	// we'd like to sort them in the order of time
	int32 NewSectionIndex = CompositeSections.Add(NewSection);

	// when first added, just make sure to link previous one to add me as next if previous one doesn't have any link
	// it's confusing first time when you add this data
	int32 PrevSectionIndex = NewSectionIndex-1;
	if ( CompositeSections.IsValidIndex(PrevSectionIndex) )
	{
		if (CompositeSections[PrevSectionIndex].NextSectionName == NAME_None)
		{
			CompositeSections[PrevSectionIndex].NextSectionName = InSectionName;
		}
	}

	return NewSectionIndex;
}

bool UAnimMontage::DeleteAnimCompositeSection(int32 SectionIndex)
{
	if ( CompositeSections.IsValidIndex(SectionIndex) )
	{
		CompositeSections.RemoveAt(SectionIndex);
		return true;
	}

	return false;
}
void UAnimMontage::SortAnimCompositeSectionByPos()
{
	// sort them in the order of time
	struct FCompareFCompositeSection
	{
		FORCEINLINE bool operator()( const FCompositeSection &A, const FCompositeSection &B ) const
		{
			return A.GetTime() < B.GetTime();
		}
	};
	CompositeSections.Sort( FCompareFCompositeSection() );
}

#endif	//WITH_EDITOR

void UAnimMontage::PostLoad()
{
	Super::PostLoad();

	// copy deprecated variable to new one, temporary code to keep data copied. Am deleting it right after this
	for ( auto SlotIter = SlotAnimTracks.CreateIterator() ; SlotIter ; ++SlotIter)
	{
		FAnimTrack & Track = (*SlotIter).AnimTrack;
		Track.ValidateSegmentTimes();

		const float CurrentCalculatedLength = CalculateSequenceLength();

		if(CurrentCalculatedLength != SequenceLength)
		{
			UE_LOG(LogAnimation, Warning, TEXT("UAnimMontage::PostLoad: The actual sequence length for montage %s does not match the length stored in the asset, please resave the montage asset."), *GetName());
			SequenceLength = CurrentCalculatedLength;
		}
	}

	int32 Ver = GetLinker()->UE4Ver();

	for(auto& Composite : CompositeSections)
	{
		if(Composite.StartTime_DEPRECATED != 0.0f)
		{
			Composite.Clear();
			Composite.LinkMontage(this, Composite.StartTime_DEPRECATED);
		}
		else
		{
			Composite.LinkMontage(this, Composite.GetTime());
		}
	}

	bool bRootMotionEnabled = bEnableRootMotionTranslation || bEnableRootMotionRotation;

	if (bRootMotionEnabled)
	{
		for (FSlotAnimationTrack& Slot : SlotAnimTracks)
		{
			for (FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
			{
				UAnimSequence * Sequence = Cast<UAnimSequence>(Segment.AnimReference);
				if (Sequence && !Sequence->bRootMotionSettingsCopiedFromMontage)
				{
					Sequence->bEnableRootMotion = true;
					Sequence->RootMotionRootLock = RootMotionRootLock;
					Sequence->bRootMotionSettingsCopiedFromMontage = true;
				}
			}
		}
	}
	// find preview base pose if it can
#if WITH_EDITORONLY_DATA
	if ( IsValidAdditive() && PreviewBasePose == NULL )
	{
		for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
		{
			if ( SlotAnimTracks[I].AnimTrack.AnimSegments.Num() > 0 )
			{
				UAnimSequence * Sequence = Cast<UAnimSequence>(SlotAnimTracks[I].AnimTrack.AnimSegments[0].AnimReference);
				if ( Sequence && Sequence->RefPoseSeq )
				{
					PreviewBasePose = Sequence->RefPoseSeq;
					MarkPackageDirty();
					break;
				}
			}
		}
	}

	// verify if skeleton matches, otherwise clear it, this can happen if anim sequence has been modified when this hasn't been loaded. 
	{
		USkeleton* MySkeleton = GetSkeleton();
		for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
		{
			if ( SlotAnimTracks[I].AnimTrack.AnimSegments.Num() > 0 )
			{
				UAnimSequence * Sequence = Cast<UAnimSequence>(SlotAnimTracks[I].AnimTrack.AnimSegments[0].AnimReference);
				if ( Sequence && Sequence->GetSkeleton() != MySkeleton )
				{
					SlotAnimTracks[I].AnimTrack.AnimSegments[0].AnimReference = 0;
					MarkPackageDirty();
					break;
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	// Register Slots w/ Skeleton
	{
		USkeleton* MySkeleton = GetSkeleton();
		if (MySkeleton)
		{
			for (int32 SlotIndex = 0; SlotIndex < SlotAnimTracks.Num(); SlotIndex++)
			{
				FName SlotName = SlotAnimTracks[SlotIndex].SlotName;
				MySkeleton->RegisterSlotNode(SlotName);
			}
		}
	}

	for(FAnimNotifyEvent& Notify : Notifies)
	{
		if(Notify.DisplayTime_DEPRECATED != 0.0f)
		{
			Notify.Clear();
			Notify.LinkMontage(this, Notify.DisplayTime_DEPRECATED);
		}
		else
		{
			Notify.LinkMontage(this, Notify.GetTime());
		}

		if(Notify.Duration != 0.0f)
		{
			Notify.EndLink.LinkMontage(this, Notify.GetTime() + Notify.Duration);
		}
	}

	// Convert BranchingPoints to AnimNotifies.
	if (GetLinker() && (GetLinker()->UE4Ver() < VER_UE4_MONTAGE_BRANCHING_POINT_REMOVAL) )
	{
		ConvertBranchingPointsToAnimNotifies();
	}
}

void UAnimMontage::ConvertBranchingPointsToAnimNotifies()
{
	if (BranchingPoints_DEPRECATED.Num() > 0)
	{
		// Handle deprecated DisplayTime first
		for (auto& BranchingPoint : BranchingPoints_DEPRECATED)
		{
			if (BranchingPoint.DisplayTime_DEPRECATED != 0.0f)
			{
				BranchingPoint.Clear();
				BranchingPoint.LinkMontage(this, BranchingPoint.DisplayTime_DEPRECATED);
			}
			else
			{
				BranchingPoint.LinkMontage(this, BranchingPoint.GetTime());
			}
		}

		// Then convert to AnimNotifies
		USkeleton * MySkeleton = GetSkeleton();

#if WITH_EDITORONLY_DATA
		// Add a new AnimNotifyTrack, and place all branching points in there.
		int32 TrackIndex = AnimNotifyTracks.Num();

		FAnimNotifyTrack NewItem;
		NewItem.TrackName = *FString::FromInt(TrackIndex + 1);
		NewItem.TrackColor = FLinearColor::White;
		AnimNotifyTracks.Add(NewItem);
#endif

		for (auto BranchingPoint : BranchingPoints_DEPRECATED)
		{
			int32 NewNotifyIndex = Notifies.Add(FAnimNotifyEvent());
			FAnimNotifyEvent& NewEvent = Notifies[NewNotifyIndex];
			NewEvent.NotifyName = BranchingPoint.EventName;

			float TriggerTime = BranchingPoint.GetTriggerTime();
			NewEvent.LinkMontage(this, TriggerTime);
#if WITH_EDITOR
			NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(CalculateOffsetForNotify(TriggerTime));
#endif
#if WITH_EDITORONLY_DATA
			NewEvent.TrackIndex = TrackIndex;
#endif
			NewEvent.Notify = NULL;
			NewEvent.NotifyStateClass = NULL;
			NewEvent.bConvertedFromBranchingPoint = true;
			NewEvent.MontageTickType = EMontageNotifyTickType::BranchingPoint;

#if WITH_EDITORONLY_DATA
			// Add as a custom AnimNotify event to Skeleton.
			if (MySkeleton)
			{
				MySkeleton->AnimationNotifies.AddUnique(NewEvent.NotifyName);
			}
#endif
		}

		BranchingPoints_DEPRECATED.Empty();
		RefreshBranchingPointMarkers();
	}
}

void UAnimMontage::RefreshBranchingPointMarkers()
{
	BranchingPointMarkers.Empty();
	BranchingPointStateNotifyIndices.Empty();

	// Verify that we have no overlapping trigger times, this is not supported, and markers would not be triggered then.
	TMap<float, FAnimNotifyEvent*> TriggerTimes;

	int32 NumNotifies = Notifies.Num();
	for (int32 NotifyIndex = 0; NotifyIndex < NumNotifies; NotifyIndex++)
	{
		FAnimNotifyEvent& NotifyEvent = Notifies[NotifyIndex];

		if (NotifyEvent.MontageTickType == EMontageNotifyTickType::BranchingPoint)
		{
			AddBranchingPointMarker(FBranchingPointMarker(NotifyIndex, NotifyEvent.GetTriggerTime(), EAnimNotifyEventType::Begin), TriggerTimes);

			if (NotifyEvent.NotifyStateClass)
			{
				// Track end point of AnimNotifyStates.
				AddBranchingPointMarker(FBranchingPointMarker(NotifyIndex, NotifyEvent.GetEndTriggerTime(), EAnimNotifyEventType::End), TriggerTimes);

				// Also track AnimNotifyStates separately, so we can tick them between their Begin and End points.
				BranchingPointStateNotifyIndices.Add(NotifyIndex);
			}
		}
	}
	
	if (BranchingPointMarkers.Num() > 0)
	{
		// Sort markers
		struct FCompareNotifyTickMarkersTime
		{
			FORCEINLINE bool operator()(const FBranchingPointMarker &A, const FBranchingPointMarker &B) const
			{
				return A.TriggerTime < B.TriggerTime;
			}
		};

		BranchingPointMarkers.Sort(FCompareNotifyTickMarkersTime());
	}
	bAnimBranchingPointNeedsSort = false;
}

void UAnimMontage::AddBranchingPointMarker(FBranchingPointMarker TickMarker, TMap<float, FAnimNotifyEvent*>& TriggerTimes)
{
	// Add Marker
	BranchingPointMarkers.Add(TickMarker);

	// Check that there is no overlapping marker, as we don't support this.
	// This would mean one of them is not getting triggered!
	FAnimNotifyEvent** FoundNotifyEventPtr = TriggerTimes.Find(TickMarker.TriggerTime);
	if (FoundNotifyEventPtr)
	{
		UE_LOG(LogAnimation, Warning, TEXT("Branching Point '%s' overlaps with '%s' at time: %f. One of them will not get triggered!"),
			*Notifies[TickMarker.NotifyIndex].NotifyName.ToString(), *(*FoundNotifyEventPtr)->NotifyName.ToString(), TickMarker.TriggerTime);
	}
	else
	{
		TriggerTimes.Add(TickMarker.TriggerTime, &Notifies[TickMarker.NotifyIndex]);
	}
}

const FBranchingPointMarker* UAnimMontage::FindFirstBranchingPointMarker(float StartTrackPos, float EndTrackPos)
{
	// Auto refresh this in editor to catch changes being made to AnimNotifies.
	// PostEditChangeProperty does not get triggered for some reason when editing notifies.
	// we need to come up with something better.
	if (bAnimBranchingPointNeedsSort || (GIsEditor && (!GetWorld() || !GetWorld()->IsPlayInEditor())))
	{
		RefreshBranchingPointMarkers();
	}

	if (BranchingPointMarkers.Num() > 0)
	{
		const bool bSearchBackwards = (EndTrackPos < StartTrackPos);
		if (!bSearchBackwards)
		{
			for (int32 Index = 0; Index < BranchingPointMarkers.Num(); Index++)
			{
				const FBranchingPointMarker& Marker = BranchingPointMarkers[Index];
				if (Marker.TriggerTime <= StartTrackPos)
				{
					continue;
				}
				if (Marker.TriggerTime > EndTrackPos)
				{
					break;
				}
				return &Marker;
			}
		}
		else
		{
			for (int32 Index = BranchingPointMarkers.Num() - 1; Index >= 0; Index--)
			{
				const FBranchingPointMarker& Marker = BranchingPointMarkers[Index];
				if (Marker.TriggerTime >= StartTrackPos)
				{
					continue;
				}
				if (Marker.TriggerTime < EndTrackPos)
				{
					break;
				}
				return &Marker;
			}
		}
	}
	return NULL;
}

void UAnimMontage::FilterOutNotifyBranchingPoints(TArray<const FAnimNotifyEvent*>& InAnimNotifies)
{
	for (int32 Index = InAnimNotifies.Num()-1; Index >= 0; Index--)
	{
		if (InAnimNotifies[Index]->MontageTickType == EMontageNotifyTickType::BranchingPoint)
		{
			InAnimNotifies.RemoveAt(Index, 1);
		}
	}
}

#if WITH_EDITOR
void UAnimMontage::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// fixme laurent - this doesn't get triggered at all when editing notifies :(
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if( PropertyName == FName(TEXT("Notifies")))
	{
		bAnimBranchingPointNeedsSort = true;
	}
}
#endif // WITH_EDITOR

bool UAnimMontage::IsValidAdditive() const
{
	// if first one is additive, this is additive
	if ( SlotAnimTracks.Num() > 0 )
	{
		for (int32 I=0; I<SlotAnimTracks.Num(); ++I)
		{
			if (!SlotAnimTracks[I].AnimTrack.IsAdditive())
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

EAnimEventTriggerOffsets::Type UAnimMontage::CalculateOffsetFromSections(float Time) const
{
	for(auto Iter = CompositeSections.CreateConstIterator(); Iter; ++Iter)
	{
		float SectionTime = Iter->GetTime();
		if(FMath::IsNearlyEqual(SectionTime,Time))
		{
			return EAnimEventTriggerOffsets::OffsetBefore;
		}
	}
	return EAnimEventTriggerOffsets::NoOffset;
}

#if WITH_EDITOR
EAnimEventTriggerOffsets::Type UAnimMontage::CalculateOffsetForNotify(float NotifyDisplayTime) const
{
	EAnimEventTriggerOffsets::Type Offset = Super::CalculateOffsetForNotify(NotifyDisplayTime);
	if(Offset == EAnimEventTriggerOffsets::NoOffset)
	{
		Offset = CalculateOffsetFromSections(NotifyDisplayTime);
	}
	return Offset;
}
#endif

bool UAnimMontage::HasRootMotion() const
{
	for (const FSlotAnimationTrack& Track : SlotAnimTracks)
	{
		if (Track.AnimTrack.HasRootMotion())
		{
			return true;
		}
	}
	return false;
}

/** Extract RootMotion Transform from a contiguous Track position range.
 * *CONTIGUOUS* means that if playing forward StartTractPosition < EndTrackPosition.
 * No wrapping over if looping. No jumping across different sections.
 * So the AnimMontage has to break the update into contiguous pieces to handle those cases.
 *
 * This does handle Montage playing backwards (StartTrackPosition > EndTrackPosition).
 *
 * It will break down the range into steps if needed to handle looping animations, or different animations.
 * These steps will be processed sequentially, and output the RootMotion transform in component space.
 */
FTransform UAnimMontage::ExtractRootMotionFromTrackRange(float StartTrackPosition, float EndTrackPosition) const
{
	FRootMotionMovementParams RootMotion;

	// For now assume Root Motion only comes from first track.
	if( SlotAnimTracks.Num() > 0 )
	{
		const FAnimTrack& SlotAnimTrack = SlotAnimTracks[0].AnimTrack;

		// Get RootMotion pieces from this track.
		// We can deal with looping animations, or multiple animations. So we break those up into sequential operations.
		// (Animation, StartFrame, EndFrame) so we can then extract root motion sequentially.
		ExtractRootMotionFromTrack(SlotAnimTrack, StartTrackPosition, EndTrackPosition, RootMotion);

	}

	UE_LOG(LogRootMotion, Log,  TEXT("\tUAnimMontage::ExtractRootMotionForTrackRange RootMotionTransform: Translation: %s, Rotation: %s"),
		*RootMotion.RootMotionTransform.GetTranslation().ToCompactString(), *RootMotion.RootMotionTransform.GetRotation().Rotator().ToCompactString() );

	return RootMotion.RootMotionTransform;
}

/** Get Montage's Group Name */
FName UAnimMontage::GetGroupName() const
{
	USkeleton* MySkeleton = GetSkeleton();
	if (MySkeleton && (SlotAnimTracks.Num() > 0))
	{
		return MySkeleton->GetSlotGroupName(SlotAnimTracks[0].SlotName);
	}

	return FAnimSlotGroup::DefaultGroupName;
}

bool UAnimMontage::HasValidSlotSetup() const
{
	// We only need to worry about this if we have multiple tracks.
	// Montages with a single track will always have a valid slot setup.
	int32 NumAnimTracks = SlotAnimTracks.Num();
	if (NumAnimTracks > 1)
	{
		USkeleton* MySkeleton = GetSkeleton();
		if (MySkeleton)
		{
			FName MontageGroupName = GetGroupName();
			TArray<FName> UniqueSlotNameList;
			UniqueSlotNameList.Add(SlotAnimTracks[0].SlotName);

			for (int32 TrackIndex = 1; TrackIndex < NumAnimTracks; TrackIndex++)
			{
				// Verify that slot names are unique.
				FName CurrentSlotName = SlotAnimTracks[TrackIndex].SlotName;
				bool bSlotNameAlreadyInUse = UniqueSlotNameList.Contains(CurrentSlotName);
				if (!bSlotNameAlreadyInUse)
				{
					UniqueSlotNameList.Add(CurrentSlotName);
				}
				else
				{
					UE_LOG(LogAnimation, Warning, TEXT("Montage '%s' not properly setup. Slot named '%s' is already used in this Montage. All slots must be unique"),
						*GetFullName(), *CurrentSlotName.ToString());
					return false;
				}

				// Verify that all slots belong to the same group.
				FName CurrentSlotGroupName = MySkeleton->GetSlotGroupName(CurrentSlotName);
				bool bDifferentGroupName = (CurrentSlotGroupName != MontageGroupName);
				if (bDifferentGroupName)
				{
					UE_LOG(LogAnimation, Warning, TEXT("Montage '%s' not properly setup. Slot's group '%s' is different than the Montage's group '%s'. All slots must belong to the same group."),
						*GetFullName(), *CurrentSlotGroupName.ToString(), *MontageGroupName.ToString());
					return false;
				}
			}
		}
	}

	return true;
}

void UAnimMontage::EvaluateCurveData(class UAnimInstance* Instance, float CurrentTime, float BlendWeight ) const
{
	Super::EvaluateCurveData(Instance, CurrentTime, BlendWeight);

	// I also need to evaluate curve of the animation
	// for now we only get the first slot
	// in the future, we'll need to do based on highest weight?
	// first get all the montage instance weight this slot node has
	if ( SlotAnimTracks.Num() > 0 )
	{
		const FAnimTrack& Track = SlotAnimTracks[0].AnimTrack;
		for (int32 I=0; I<Track.AnimSegments.Num(); ++I)
		{
			const FAnimSegment& AnimSegment = Track.AnimSegments[I];

			float PositionInAnim = 0.f;
			float Weight = 0.f;
			UAnimSequenceBase* AnimRef = AnimSegment.GetAnimationData(CurrentTime, PositionInAnim, Weight);
			// make this to be 1 function
			if ( AnimRef && Weight > ZERO_ANIMWEIGHT_THRESH )
			{
				// todo anim: hack - until we fix animcomposite
				UAnimSequence * Sequence = Cast<UAnimSequence>(AnimRef);
				if ( Sequence )
				{
					Sequence->EvaluateCurveData(Instance, PositionInAnim, BlendWeight*Weight);
				}
			}
		}	
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// MontageInstance
/////////////////////////////////////////////////////////////////////////////////////////////

void FAnimMontageInstance::Play(float InPlayRate)
{
	bPlaying = true;
	PlayRate = InPlayRate;
	if( Montage )
	{
		BlendTime = Montage->BlendInTime * DefaultBlendTimeMultiplier;
	}
	DesiredWeight = 1.f;
}

void FAnimMontageInstance::Stop(float BlendOutDuration, bool bInterrupt)
{
	// overwrite bInterrupted if it hasn't already interrupted
	// once interrupted, you don't go back to non-interrupted
	if (!bInterrupted && bInterrupt)
	{
		bInterrupted = bInterrupt;
	}

	if (DesiredWeight > 0.f)
	{
		DesiredWeight = 0.f;
		if (Montage)
		{
			// do not use default Montage->BlendOut  Time
			// depending on situation, the BlendOut time changes
			// check where this function gets called and see how we calculate BlendTime
			BlendTime = BlendOutDuration;

			if (UAnimInstance* Inst = AnimInstance.Get())
			{
				// Let AnimInstance know we are being stopped.
				Inst->OnMontageInstanceStopped(*this);
				Inst->QueueMontageBlendingOutEvent(FQueuedMontageBlendingOutEvent(Montage, bInterrupted, OnMontageBlendingOutStarted));
			}
		}
	}
	else
	{
		// it is already stopped, but new montage blendtime is shorter than what 
		// I'm blending out, that means this needs to readjust blendtime
		// that way we don't accumulate old longer blendtime for newer montage to play
		if (BlendOutDuration < BlendTime)
		{
			BlendTime = BlendOutDuration;
		}
	}

	if (BlendTime <= 0.0f)
	{
		bPlaying = false;
	}
}

void FAnimMontageInstance::Pause()
{
	bPlaying = false;
}

void FAnimMontageInstance::Initialize(class UAnimMontage * InMontage)
{
	if (InMontage)
	{
		Montage = InMontage;
		Position = 0.f;
		DesiredWeight = 1.f;

		RefreshNextPrevSections();
	}
}

void FAnimMontageInstance::RefreshNextPrevSections()
{
	// initialize next section
	if ( Montage->CompositeSections.Num() > 0 )
	{
		NextSections.Empty(Montage->CompositeSections.Num());
		NextSections.AddUninitialized(Montage->CompositeSections.Num());
		PrevSections.Empty(Montage->CompositeSections.Num());
		PrevSections.AddUninitialized(Montage->CompositeSections.Num());

		for (int32 I=0; I<Montage->CompositeSections.Num(); ++I)
		{
			PrevSections[I] = INDEX_NONE;
		}

		for (int32 I=0; I<Montage->CompositeSections.Num(); ++I)
		{
			FCompositeSection & Section = Montage->CompositeSections[I];
			int32 NextSectionIdx = Montage->GetSectionIndex(Section.NextSectionName);
			NextSections[I] = NextSectionIdx;
			if (NextSections.IsValidIndex(NextSectionIdx))
			{
				PrevSections[NextSectionIdx] = I;
			}
		}
	}
}

void FAnimMontageInstance::AddReferencedObjects( FReferenceCollector& Collector )
{
	if (Montage)
	{
		Collector.AddReferencedObject(Montage);
	}
}

void FAnimMontageInstance::Terminate()
{
	if (Montage == NULL)
	{
		return;
	}

	UAnimMontage* OldMontage = Montage;
	Montage = NULL;

	UAnimInstance* Inst = AnimInstance.Get();
	if (Inst)
	{
		// End all active State BranchingPoints
		for (int32 Index = ActiveStateBranchingPoints.Num() - 1; Index >= 0; Index--)
		{
			FAnimNotifyEvent& NotifyEvent = ActiveStateBranchingPoints[Index];
			NotifyEvent.NotifyStateClass->NotifyEnd(Inst->GetSkelMeshComponent(), Cast<UAnimSequenceBase>(NotifyEvent.NotifyStateClass->GetOuter()));
		}
		ActiveStateBranchingPoints.Empty();

		// terminating, trigger end
		Inst->QueueMontageEndedEvent(FQueuedMontageEndedEvent(OldMontage, bInterrupted, OnMontageEnded));
	}

	// Clear any active synchronization
	MontageSync_StopFollowing();
	MontageSync_StopLeading();
}

bool FAnimMontageInstance::JumpToSectionName(FName const & SectionName, bool bEndOfSection)
{
	const int32 SectionID = Montage->GetSectionIndex(SectionName);

	if (Montage->IsValidSectionIndex(SectionID))
	{
		FCompositeSection & CurSection = Montage->GetAnimCompositeSection(SectionID);
		Position = Montage->CalculatePos(CurSection, bEndOfSection ? Montage->GetSectionLength(SectionID) - KINDA_SMALL_NUMBER : 0.0f);
		OnMontagePositionChanged(SectionName);
		return true;
	}

	UE_LOG(LogAnimation, Warning, TEXT("JumpToSectionName %s bEndOfSection: %d failed for Montage %s"),
		*SectionName.ToString(), bEndOfSection, *GetNameSafe(Montage));
	return false;
}

bool FAnimMontageInstance::SetNextSectionName(FName const & SectionName, FName const & NewNextSectionName)
{
	int32 const SectionID = Montage->GetSectionIndex(SectionName);
	int32 const NewNextSectionID = Montage->GetSectionIndex(NewNextSectionName);

	return SetNextSectionID(SectionID, NewNextSectionID);
}

bool FAnimMontageInstance::SetNextSectionID(int32 const & SectionID, int32 const & NewNextSectionID)
{
	bool const bHasValidNextSection = NextSections.IsValidIndex(SectionID);

	// disconnect prev section
	if (bHasValidNextSection && (NextSections[SectionID] != INDEX_NONE) && PrevSections.IsValidIndex(NextSections[SectionID]))
	{
		PrevSections[NextSections[SectionID]] = INDEX_NONE;
	}

	// update in-reverse next section
	if (PrevSections.IsValidIndex(NewNextSectionID))
	{
		PrevSections[NewNextSectionID] = SectionID;
	}

	// update next section for the SectionID
	// NextSection can be invalid
	if (bHasValidNextSection)
	{
		NextSections[SectionID] = NewNextSectionID;
		OnMontagePositionChanged(GetSectionNameFromID(NewNextSectionID));
		return true;
	}

	UE_LOG(LogAnimation, Warning, TEXT("SetNextSectionName %s to %s failed for Montage %s"),
		*GetSectionNameFromID(SectionID).ToString(), *GetSectionNameFromID(NewNextSectionID).ToString(), *GetNameSafe(Montage));

	return false;
}

void FAnimMontageInstance::OnMontagePositionChanged(FName const & ToSectionName) 
{
	if (bPlaying && (DesiredWeight == 0.f))
	{
		UE_LOG(LogAnimation, Warning, TEXT("Changing section on Montage (%s) to '%s' during blend out. This can cause incorrect visuals!"),
			*GetNameSafe(Montage), *ToSectionName.ToString());

		Play(PlayRate);
	}
}

FName FAnimMontageInstance::GetCurrentSection() const
{
	if ( Montage )
	{
		float CurrentPosition;
		const int32 CurrentSectionIndex = Montage->GetAnimCompositeSectionIndexFromPos(Position, CurrentPosition);
		if ( Montage->IsValidSectionIndex(CurrentSectionIndex) )
		{
			FCompositeSection& CurrentSection = Montage->GetAnimCompositeSection(CurrentSectionIndex);
			return CurrentSection.SectionName;
		}
	}

	return NAME_None;
}

FName FAnimMontageInstance::GetNextSection() const
{
	if (Montage)
	{
		float CurrentPosition;
		const int32 CurrentSectionIndex = Montage->GetAnimCompositeSectionIndexFromPos(Position, CurrentPosition);
		if (Montage->IsValidSectionIndex(CurrentSectionIndex))
		{
			FCompositeSection& CurrentSection = Montage->GetAnimCompositeSection(CurrentSectionIndex);
			return CurrentSection.NextSectionName;
		}
	}

	return NAME_None;
}

int32 FAnimMontageInstance::GetNextSectionID(int32 const & CurrentSectionID) const
{
	return (IsActive() && (CurrentSectionID < NextSections.Num())) ? NextSections[CurrentSectionID] : INDEX_NONE;
}

FName FAnimMontageInstance::GetSectionNameFromID(int32 const & SectionID) const
{
	if (Montage && Montage->IsValidSectionIndex(SectionID))
	{
		FCompositeSection const & CurrentSection = Montage->GetAnimCompositeSection(SectionID);
		return CurrentSection.SectionName;
	}

	return NAME_None;
}

void FAnimMontageInstance::MontageSync_Follow(struct FAnimMontageInstance* NewLeaderMontageInstance)
{
	// Stop following previous leader if any.
	MontageSync_StopFollowing();

	// Follow new leader
	// Note: we don't really care about detecting loops there, there's no real harm in doing so.
	if (NewLeaderMontageInstance && (NewLeaderMontageInstance != this))
	{
		NewLeaderMontageInstance->MontageSyncFollowers.AddUnique(this);
		MontageSyncLeader = NewLeaderMontageInstance;
	}
}

void FAnimMontageInstance::MontageSync_StopLeading()
{
	for (auto MontageSyncFollower : MontageSyncFollowers)
	{
		if (MontageSyncFollower)
		{
			ensure(MontageSyncFollower->MontageSyncLeader == this);
			MontageSyncFollower->MontageSyncLeader = NULL;
		}
	}
	MontageSyncFollowers.Empty();
}

void FAnimMontageInstance::MontageSync_StopFollowing()
{
	if (MontageSyncLeader)
	{
		MontageSyncLeader->MontageSyncFollowers.RemoveSingleSwap(this);
		MontageSyncLeader = NULL;
	}
}

uint32 FAnimMontageInstance::MontageSync_GetFrameCounter() const
{
	return (GFrameCounter % MAX_uint32);
}

bool FAnimMontageInstance::MontageSync_HasBeenUpdatedThisFrame() const
{
	return (MontageSyncUpdateFrameCounter == MontageSync_GetFrameCounter());
}

void FAnimMontageInstance::MontageSync_PreUpdate()
{
	// If we are being synchronized to a leader
	// And our leader HASN'T been updated yet, then we need to synchronize ourselves now.
	// We're basically synchronizing to last frame's values.
	// If we want to avoid that frame of lag, a tick prerequisite should be put between the follower and the leader.
	if (MontageSyncLeader && !MontageSyncLeader->MontageSync_HasBeenUpdatedThisFrame())
	{
		MontageSync_PerformSyncToLeader();
	}
}

void FAnimMontageInstance::MontageSync_PostUpdate()
{
	// Tag ourselves as updated this frame.
	MontageSyncUpdateFrameCounter = MontageSync_GetFrameCounter();

	// If we are being synchronized to a leader
	// And our leader HAS already been updated, then we can synchronize ourselves now.
	// To make sure we are in sync before rendering.
	if (MontageSyncLeader && MontageSyncLeader->MontageSync_HasBeenUpdatedThisFrame())
	{
		MontageSync_PerformSyncToLeader();
	}
}

void FAnimMontageInstance::MontageSync_PerformSyncToLeader()
{
	if (MontageSyncLeader)
	{
		// Sync follower position only if significant error.
		// We don't want continually 'teleport' it, which could have side-effects and skip AnimNotifies.
		const float LeaderPosition = MontageSyncLeader->GetPosition();
		const float FollowerPosition = GetPosition();
		if (FMath::Abs(FollowerPosition - LeaderPosition) > KINDA_SMALL_NUMBER)
		{
			SetPosition(LeaderPosition);
		}

		SetPlayRate(MontageSyncLeader->GetPlayRate());

		// If source and target share same section names, keep them in sync as well. So we properly handle jumps and loops.
		const FName LeaderCurrentSectionName = MontageSyncLeader->GetCurrentSection();
		if ((LeaderCurrentSectionName != NAME_None) && (GetCurrentSection() == LeaderCurrentSectionName))
		{
			const FName LeaderNextSectionName = MontageSyncLeader->GetNextSection();
			SetNextSectionName(LeaderCurrentSectionName, LeaderNextSectionName);
		}
	}
}

void FAnimMontageInstance::UpdateWeight(float DeltaTime)
{
	if ( IsValid() )
	{
		PreviousWeight = Weight;

		// update weight
		FAnimationRuntime::TickBlendWeight(DeltaTime, DesiredWeight, Weight, BlendTime);

		// Notify weight is max of previous and current as notify could have come
		// from any point between now and last tick
		NotifyWeight = FMath::Max(PreviousWeight, Weight);
	}
}

bool FAnimMontageInstance::SimulateAdvance(float DeltaTime, float& InOutPosition, FRootMotionMovementParams & OutRootMotionParams) const
{
	if( !IsValid() )
	{
		return false;
	}

	const float CombinedPlayRate = PlayRate * Montage->RateScale;
	const bool bPlayingForward = (CombinedPlayRate > 0.f);

	const bool bExtractRootMotion = Montage->HasRootMotion();

	float DesiredDeltaMove = CombinedPlayRate * DeltaTime;
	float OriginalMoveDelta = DesiredDeltaMove;

	while( (FMath::Abs(DesiredDeltaMove) > KINDA_SMALL_NUMBER) && ((OriginalMoveDelta * DesiredDeltaMove) > 0.f) )
	{
		// Get position relative to current montage section.
		float PosInSection;
		int32 CurrentSectionIndex = Montage->GetAnimCompositeSectionIndexFromPos(InOutPosition, PosInSection);

		if( Montage->IsValidSectionIndex(CurrentSectionIndex) )
		{
			const FCompositeSection& CurrentSection = Montage->GetAnimCompositeSection(CurrentSectionIndex);

			// we need to advance within section
			check(NextSections.IsValidIndex(CurrentSectionIndex));
			const int32 NextSectionIndex = bPlayingForward ? NextSections[CurrentSectionIndex] : PrevSections[CurrentSectionIndex];

			// Find end of current section. We only update one section at a time.
			const float CurrentSectionLength = Montage->GetSectionLength(CurrentSectionIndex);
			float StoppingPosInSection = CurrentSectionLength;

			// Also look for a branching point. If we have one, stop there first to handle it.
			const float CurrentSectionEndPos = CurrentSection.GetTime() + CurrentSectionLength;

			// Update position within current section.
			// Note that we explicitly disallow looping there, we handle it ourselves to simplify code by not having to handle time wrapping around in multiple places.
			const float PrevPosInSection = PosInSection;
			const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, PosInSection, StoppingPosInSection);
			// ActualDeltaPos == ActualDeltaMove since we break down looping in multiple steps. So we don't have to worry about time wrap around here.
			const float ActualDeltaPos = PosInSection - PrevPosInSection;
			const float ActualDeltaMove = ActualDeltaPos;

			// Decrease actual move from desired move. We'll take another iteration if there is anything left.
			DesiredDeltaMove -= ActualDeltaMove;

			float PrevPosition = Position;
			InOutPosition = FMath::Clamp<float>(InOutPosition + ActualDeltaPos, CurrentSection.GetTime(), CurrentSectionEndPos);

			if( FMath::Abs(ActualDeltaMove) > 0.f )
			{
				// Extract Root Motion for this time slice, and accumulate it.
				if( bExtractRootMotion )
				{
					OutRootMotionParams.Accumulate(Montage->ExtractRootMotionFromTrackRange(PrevPosition, InOutPosition));
				}

				// if we reached end of section, and we were not processing a branching point, and no events has messed with out current position..
				// .. Move to next section.
				// (this also handles looping, the same as jumping to a different section).
				if( AdvanceType != ETAA_Default )
				{
					// Get recent NextSectionIndex in case it's been changed by previous events.
					const int32 RecentNextSectionIndex = bPlayingForward ? NextSections[CurrentSectionIndex] : PrevSections[CurrentSectionIndex];
					if( RecentNextSectionIndex != INDEX_NONE )
					{
						float LatestNextSectionStartTime;
						float LatestNextSectionEndTime;
						Montage->GetSectionStartAndEndTime(RecentNextSectionIndex, LatestNextSectionStartTime, LatestNextSectionEndTime);
						// Jump to next section's appropriate starting point (start or end).
						InOutPosition = bPlayingForward ? LatestNextSectionStartTime : (LatestNextSectionEndTime - KINDA_SMALL_NUMBER); // remain within section
					}
				}
			}
			else
			{
				break;
			}
		}
		else
		{
			// stop and leave this loop
			break;
		}
	}

	return true;
}

void FAnimMontageInstance::Advance(float DeltaTime, struct FRootMotionMovementParams * OutRootMotionParams, bool bBlendRootMotion)
{
	if( IsValid() )
	{
#if WITH_EDITOR
		// this is necessary and it is not easy to do outside of here
		// since undo also can change composite sections
		if( (Montage->CompositeSections.Num() != NextSections.Num()) || (Montage->CompositeSections.Num() != PrevSections.Num()))
		{
			RefreshNextPrevSections();
		}
#endif

		// if no weight, no reason to update, and if not playing, we don't need to advance
		// this portion is to advance position
		// If we just reached zero weight, still tick this frame to fire end of animation events.
		if( bPlaying && (Weight > ZERO_ANIMWEIGHT_THRESH || PreviousWeight > ZERO_ANIMWEIGHT_THRESH) )
		{
			const float CombinedPlayRate = PlayRate * Montage->RateScale;
			const bool bPlayingForward = (CombinedPlayRate > 0.f);

			const bool bExtractRootMotion = (OutRootMotionParams != NULL) && Montage->HasRootMotion();
			
			float DesiredDeltaMove = CombinedPlayRate * DeltaTime;
			float OriginalMoveDelta = DesiredDeltaMove;

			while( bPlaying && (FMath::Abs(DesiredDeltaMove) > KINDA_SMALL_NUMBER) && ((OriginalMoveDelta * DesiredDeltaMove) > 0.f) )
			{
				// Get position relative to current montage section.
				float PosInSection;
				int32 CurrentSectionIndex = Montage->GetAnimCompositeSectionIndexFromPos(Position, PosInSection);
				
				if( Montage->IsValidSectionIndex(CurrentSectionIndex) )
				{
					const FCompositeSection& CurrentSection = Montage->GetAnimCompositeSection(CurrentSectionIndex);

					// we need to advance within section
					check(NextSections.IsValidIndex(CurrentSectionIndex));
					const int32 NextSectionIndex = bPlayingForward ? NextSections[CurrentSectionIndex] : PrevSections[CurrentSectionIndex];

					// Find end of current section. We only update one section at a time.
					const float CurrentSectionLength = Montage->GetSectionLength(CurrentSectionIndex);
					float StoppingPosInSection = CurrentSectionLength;

					// Also look for a branching point. If we have one, stop there first to handle it.
					const float CurrentSectionEndPos = CurrentSection.GetTime() + CurrentSectionLength;
					const FBranchingPointMarker* BranchingPointMarker = Montage->FindFirstBranchingPointMarker(Position, FMath::Clamp(Position + DesiredDeltaMove, CurrentSection.GetTime(), CurrentSectionEndPos));
					if (BranchingPointMarker)
					{
						// get the first one to see if it's less than AnimEnd
						StoppingPosInSection = BranchingPointMarker->TriggerTime - CurrentSection.GetTime();
					}

					// Update position within current section.
					// Note that we explicitly disallow looping there, we handle it ourselves to simplify code by not having to handle time wrapping around in multiple places.
					const float PrevPosInSection = PosInSection;
					const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, PosInSection, StoppingPosInSection);
					// ActualDeltaPos == ActualDeltaMove since we break down looping in multiple steps. So we don't have to worry about time wrap around here.
					const float ActualDeltaPos = PosInSection - PrevPosInSection;
					const float ActualDeltaMove = ActualDeltaPos;

					// Decrease actual move from desired move. We'll take another iteration if there is anything left.
					DesiredDeltaMove -= ActualDeltaMove;

					float PrevPosition = Position;
					Position = FMath::Clamp<float>(Position + ActualDeltaPos, CurrentSection.GetTime(), CurrentSectionEndPos);

					const float PositionBeforeFiringEvents = Position;

					const bool bHaveMoved = FMath::Abs(ActualDeltaMove) > 0.f;

					if (bHaveMoved)
					{
						// Extract Root Motion for this time slice, and accumulate it.
						if (bExtractRootMotion && AnimInstance.IsValid())
						{
							FTransform RootMotion = Montage->ExtractRootMotionFromTrackRange(PrevPosition, Position);
							if (bBlendRootMotion)
							{
								const float RootMotionSlotWeight = AnimInstance.Get()->GetSlotRootMotionWeight(Montage->SlotAnimTracks[0].SlotName);
								const float RootMotionInstanceWeight = Weight * RootMotionSlotWeight;
								OutRootMotionParams->AccumulateWithBlend(RootMotion, RootMotionInstanceWeight);
							}
							else
							{
								OutRootMotionParams->Accumulate(RootMotion);
							}
						}
					}

					// If current section is last one, check to trigger a blend out.
					if( NextSectionIndex == INDEX_NONE )
					{
						const float DeltaPosToEnd = bPlayingForward ? (CurrentSectionLength - PosInSection) : PosInSection;
						const float DeltaTimeToEnd = DeltaPosToEnd / FMath::Abs(CombinedPlayRate);

						const bool bCustomBlendOutTriggerTime = (Montage->BlendOutTriggerTime >= 0);
						const float DefaultBlendOutTime = Montage->BlendOutTime * DefaultBlendTimeMultiplier;
						const float BlendOutTriggerTime = bCustomBlendOutTriggerTime ? Montage->BlendOutTriggerTime : DefaultBlendOutTime;
							
						// ... trigger blend out if within blend out time window.
						if (DeltaTimeToEnd <= FMath::Max<float>(BlendOutTriggerTime, KINDA_SMALL_NUMBER))
						{
							const float BlendOutTime = bCustomBlendOutTriggerTime ? DefaultBlendOutTime : DeltaTimeToEnd;
							Stop(BlendOutTime, false);
						}
					}

					if (bHaveMoved)
					{
						// Delegate has to be called last in this loop
						// so that if this changes position, the new position will be applied in the next loop
						// first need to have event handler to handle it

						// Save position before firing events.
						if (!bInterrupted)
						{
							HandleEvents(PrevPosition, Position, BranchingPointMarker);
						}
					}

					// if we reached end of section, and we were not processing a branching point, and no events has messed with out current position..
					// .. Move to next section.
					// (this also handles looping, the same as jumping to a different section).
					if ((AdvanceType != ETAA_Default) && !BranchingPointMarker && (PositionBeforeFiringEvents == Position))
					{
						// Get recent NextSectionIndex in case it's been changed by previous events.
						const int32 RecentNextSectionIndex = bPlayingForward ? NextSections[CurrentSectionIndex] : PrevSections[CurrentSectionIndex];

						if( RecentNextSectionIndex != INDEX_NONE )
						{
							float LatestNextSectionStartTime, LatestNextSectionEndTime;
							Montage->GetSectionStartAndEndTime(RecentNextSectionIndex, LatestNextSectionStartTime, LatestNextSectionEndTime);

							// Jump to next section's appropriate starting point (start or end).
							float EndOffset = KINDA_SMALL_NUMBER/2.f; //KINDA_SMALL_NUMBER/2 because we use KINDA_SMALL_NUMBER to offset notifies for triggering and SMALL_NUMBER is too small
							Position = bPlayingForward ? LatestNextSectionStartTime : (LatestNextSectionEndTime - EndOffset);
						}
					}
				}
				else
				{
					// stop and leave this loop
					Stop(Montage->BlendOutTime * DefaultBlendTimeMultiplier, false);
					break;
				}
			}
		}
	}

	// If this Montage has no weight, it should be terminated.
	if ((Weight <= ZERO_ANIMWEIGHT_THRESH) && (DesiredWeight <= ZERO_ANIMWEIGHT_THRESH))
	{
		// nothing else to do
		Terminate();
		return;
	}

	// Update curves based on final position.
	if( (Weight > ZERO_ANIMWEIGHT_THRESH) && IsValid() )
	{
		Montage->EvaluateCurveData(AnimInstance.Get(), Position, Weight);
	}

	if (!bInterrupted)
	{
		// Tick all active state branching points
		for (int32 Index = 0; Index < ActiveStateBranchingPoints.Num(); Index++)
		{
			FAnimNotifyEvent& NotifyEvent = ActiveStateBranchingPoints[Index];
			NotifyEvent.NotifyStateClass->NotifyTick(AnimInstance->GetSkelMeshComponent(), Montage, DeltaTime);
		}
	}
}

void FAnimMontageInstance::HandleEvents(float PreviousTrackPos, float CurrentTrackPos, const FBranchingPointMarker* BranchingPointMarker)
{
	// Skip notifies and branching points if montage has been interrupted.
	if (bInterrupted)
	{
		return;
	}

	// now get active Notifies based on how it advanced
	if (AnimInstance.IsValid())
	{
		TArray<const FAnimNotifyEvent*> Notifies;

		// We already break up AnimMontage update to handle looping, so we guarantee that PreviousPos and CurrentPos are contiguous.
		Montage->GetAnimNotifiesFromDeltaPositions(PreviousTrackPos, CurrentTrackPos, Notifies);

		// For Montage only, remove notifies marked as 'branching points'. They are not queued and are handled separately.
		Montage->FilterOutNotifyBranchingPoints(Notifies);

		// now trigger notifies for all animations within montage
		// we'll do this for all slots for now
		for (auto SlotTrack = Montage->SlotAnimTracks.CreateIterator(); SlotTrack; ++SlotTrack)
		{
			if (AnimInstance->IsSlotNodeRelevantForNotifies(SlotTrack->SlotName))
			{
				for (auto AnimSegment = SlotTrack->AnimTrack.AnimSegments.CreateIterator(); AnimSegment; ++AnimSegment)
				{
					AnimSegment->GetAnimNotifiesFromTrackPositions(PreviousTrackPos, CurrentTrackPos, Notifies);
				}
			}
		}

		// Queue all these notifies.
		AnimInstance->AddAnimNotifies(Notifies, NotifyWeight);
	}

	// Update active state branching points, before we handle the immediate tick marker.
	// In case our position jumped on the timeline, we need to begin/end state branching points accordingly.
	UpdateActiveStateBranchingPoints(CurrentTrackPos);

	// Trigger ImmediateTickMarker event if we have one
	if (BranchingPointMarker)
	{
		BranchingPointEventHandler(BranchingPointMarker);
	}
}

void FAnimMontageInstance::UpdateActiveStateBranchingPoints(float CurrentTrackPosition)
{
	int32 NumStateBranchingPoints = Montage->BranchingPointStateNotifyIndices.Num();
	if (NumStateBranchingPoints > 0)
	{
		// End no longer active events first. We want this to happen before we trigger NotifyBegin on newly active events.
		for (int32 Index = ActiveStateBranchingPoints.Num() - 1; Index >= 0; Index--)
		{
			FAnimNotifyEvent& NotifyEvent = ActiveStateBranchingPoints[Index];

			const float NotifyStartTime = NotifyEvent.GetTriggerTime();
			const float NotifyEndTime = NotifyEvent.GetEndTriggerTime();
			bool bNotifyIsActive = (CurrentTrackPosition > NotifyStartTime) && (CurrentTrackPosition <= NotifyEndTime);

			if (!bNotifyIsActive)
			{
				NotifyEvent.NotifyStateClass->NotifyEnd(AnimInstance->GetSkelMeshComponent(), Cast<UAnimSequenceBase>(NotifyEvent.NotifyStateClass->GetOuter()));
				ActiveStateBranchingPoints.RemoveAt(Index, 1);
			}
		}

		// Then, begin newly active notifies
		for (int32 Index = 0; Index < NumStateBranchingPoints; Index++)
		{
			const int32 NotifyIndex = Montage->BranchingPointStateNotifyIndices[Index];
			FAnimNotifyEvent& NotifyEvent = Montage->Notifies[NotifyIndex];

			const float NotifyStartTime = NotifyEvent.GetTriggerTime();
			const float NotifyEndTime = NotifyEvent.GetEndTriggerTime();

			bool bNotifyIsActive = (CurrentTrackPosition > NotifyStartTime) && (CurrentTrackPosition <= NotifyEndTime);
			if (bNotifyIsActive && !ActiveStateBranchingPoints.Contains(NotifyEvent))
			{
				NotifyEvent.NotifyStateClass->NotifyBegin(AnimInstance->GetSkelMeshComponent(), Cast<UAnimSequenceBase>(NotifyEvent.NotifyStateClass->GetOuter()), NotifyEvent.GetDuration());
				ActiveStateBranchingPoints.Add(NotifyEvent);
			}
		}
	}
}

void FAnimMontageInstance::BranchingPointEventHandler(const FBranchingPointMarker* BranchingPointMarker)
{
	if (AnimInstance.IsValid() && Montage && BranchingPointMarker)
	{
		FAnimNotifyEvent* NotifyEvent = (BranchingPointMarker->NotifyIndex < Montage->Notifies.Num()) ? &Montage->Notifies[BranchingPointMarker->NotifyIndex] : NULL;
		if (NotifyEvent)
		{
			// Handle backwards compatibility with older BranchingPoints.
			if (NotifyEvent->bConvertedFromBranchingPoint && (NotifyEvent->NotifyName != NAME_None))
			{
				FString FuncName = FString::Printf(TEXT("MontageBranchingPoint_%s"), *NotifyEvent->NotifyName.ToString());
				FName FuncFName = FName(*FuncName);

				UFunction* Function = AnimInstance.Get()->FindFunction(FuncFName);
				if (Function)
				{
					AnimInstance.Get()->ProcessEvent(Function, NULL);
				}
				// In case older BranchingPoint has been re-implemented as a new Custom Notify, this is if BranchingPoint function hasn't been found.
				else
				{
					AnimInstance.Get()->TriggerSingleAnimNotify(NotifyEvent);
				}
			}
			else if (NotifyEvent->NotifyStateClass != NULL)
			{
				if (BranchingPointMarker->NotifyEventType == EAnimNotifyEventType::Begin)
				{
					NotifyEvent->NotifyStateClass->NotifyBegin(AnimInstance->GetSkelMeshComponent(), Cast<UAnimSequenceBase>(NotifyEvent->NotifyStateClass->GetOuter()), NotifyEvent->GetDuration());
					ActiveStateBranchingPoints.Add(*NotifyEvent);
				}
				else
				{
					NotifyEvent->NotifyStateClass->NotifyEnd(AnimInstance->GetSkelMeshComponent(), Cast<UAnimSequenceBase>(NotifyEvent->NotifyStateClass->GetOuter()));
					ActiveStateBranchingPoints.RemoveSingleSwap(*NotifyEvent);
				}
			}
			// Regular 'non state' anim notifies
			else
			{
				AnimInstance.Get()->TriggerSingleAnimNotify(NotifyEvent);
			}
		}
	}
}

float UAnimMontage::CalculateSequenceLength()
{
	float CalculatedSequenceLength = 0.f;
	for(auto Iter = SlotAnimTracks.CreateIterator(); Iter; ++Iter)
	{
		FSlotAnimationTrack& SlotAnimTrack = (*Iter);
		if(SlotAnimTrack.AnimTrack.AnimSegments.Num() > 0)
		{
			CalculatedSequenceLength = FMath::Max(CalculatedSequenceLength, SlotAnimTrack.AnimTrack.GetLength());
		}
	}
	return CalculatedSequenceLength;
}

#if WITH_EDITOR
bool UAnimMontage::GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences)
{
 	for (auto Iter=SlotAnimTracks.CreateConstIterator(); Iter; ++Iter)
 	{
 		const FSlotAnimationTrack& Track = (*Iter);
 		Track.AnimTrack.GetAllAnimationSequencesReferred(AnimationSequences);
 	}
 	return (AnimationSequences.Num() > 0);
}

void UAnimMontage::ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap)
{
	for (auto Iter=SlotAnimTracks.CreateIterator(); Iter; ++Iter)
	{
		FSlotAnimationTrack& Track = (*Iter);
		Track.AnimTrack.ReplaceReferredAnimations(ReplacementMap);
	}
}

ENGINE_API void UAnimMontage::UpdateLinkableElements()
{
	// Update all linkable elements
	for(FCompositeSection& Section : CompositeSections)
	{
		Section.Update();
	}

	for(FAnimNotifyEvent& Notify : Notifies)
	{
		Notify.Update();
		Notify.RefreshTriggerOffset(CalculateOffsetForNotify(Notify.GetTime()));

		Notify.EndLink.Update();
		Notify.RefreshEndTriggerOffset(CalculateOffsetForNotify(Notify.EndLink.GetTime()));
	}
}

ENGINE_API void UAnimMontage::UpdateLinkableElements(int32 SlotIdx, int32 SegmentIdx)
{
	FAnimSegment* UpdatedSegment = &SlotAnimTracks[SlotIdx].AnimTrack.AnimSegments[SegmentIdx];

	for(FCompositeSection& Section : CompositeSections)
	{
		if(Section.GetSlotIndex() == SlotIdx && Section.GetSegmentIndex() == SegmentIdx)
		{
			// Update the link
			Section.Update();
		}
	}

	for(FAnimNotifyEvent& Notify : Notifies)
	{
		if(Notify.GetSlotIndex() == SlotIdx && Notify.GetSegmentIndex() == SegmentIdx)
		{
			Notify.Update();
			Notify.RefreshTriggerOffset(CalculateOffsetForNotify(Notify.GetTime()));
		}

		if(Notify.EndLink.GetSlotIndex() == SlotIdx && Notify.EndLink.GetSegmentIndex() == SegmentIdx)
		{
			Notify.EndLink.Update();
			Notify.RefreshEndTriggerOffset(CalculateOffsetForNotify(Notify.EndLink.GetTime()));
		}
	}
}

#endif
