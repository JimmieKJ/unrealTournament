// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AnimationUtils.h"
#include "AnimationRuntime.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimInstance.h"


DEFINE_LOG_CATEGORY(LogAnimMarkerSync);

DECLARE_CYCLE_STAT(TEXT("AnimSeq EvalCurveData"), STAT_AnimSeq_EvalCurveData, STATGROUP_Anim);


/////////////////////////////////////////////////////

UAnimSequenceBase::UAnimSequenceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RateScale(1.0f)
{
}

template <typename DataType>
void UAnimSequenceBase::VerifyCurveNames(USkeleton* Skeleton, const FName& NameContainer, TArray<DataType>& CurveList)
{
	FSmartNameMapping* NameMapping = Skeleton->SmartNames.GetContainer(NameContainer);

	// since this is verify function that makes sure it exists after loaded
	// we should add it if it doesn't exist
	if (!NameMapping)
	{
		// if it doens't exists, we should add it
		Skeleton->Modify(true);
		Skeleton->SmartNames.AddContainer(NameContainer);
		NameMapping = Skeleton->SmartNames.GetContainer(NameContainer);
		check(NameMapping);
	}

	TArray<DataType*> UnlinkedCurves;
	for(DataType& Curve : CurveList)
	{
		const FSmartNameMapping::UID* UID = NameMapping->FindUID(Curve.LastObservedName);
		if(!UID)
		{
			// The skeleton doesn't know our name. Use the last observed name that was saved with the
			// curve to create a new name. This can happen if a user saves an animation but not a skeleton
			// either when updating the assets or editing the curves within.
			UnlinkedCurves.Add(&Curve);
		}
		else if (Curve.CurveUid != *UID)// we verify if UID is correct
		{
			// if UID doesn't match, this is suspicious
			// because same name but different UID is not idea
			// so we'll fix up UID
			Curve.CurveUid = *UID;
		}
	}

	for(DataType* Curve : UnlinkedCurves)
	{
		NameMapping->AddOrFindName(Curve->LastObservedName, Curve->CurveUid);
	}
}

void UAnimSequenceBase::PostLoad()
{
	Super::PostLoad();

	// Convert Notifies to new data
	if( GIsEditor && Notifies.Num() > 0 )
	{
		if(GetLinkerUE4Version() < VER_UE4_CLEAR_NOTIFY_TRIGGERS)
		{
			for(FAnimNotifyEvent Notify : Notifies)
			{
				if(Notify.Notify)
				{
					// Clear end triggers for notifies that are not notify states
					Notify.EndTriggerTimeOffset = 0.0f;
				}
			}
		}
	}

	// Ensure notifies are sorted.
	SortNotifies();

#if WITH_EDITOR
	InitializeNotifyTrack();
#endif
	RefreshCacheData();

	if(USkeleton* Skeleton = GetSkeleton())
	{
		// Fix up the existing curves to work with smartnames
		if(GetLinkerUE4Version() < VER_UE4_SKELETON_ADD_SMARTNAMES)
		{
			// Get the name mapping object for curves
			FSmartNameMapping* NameMapping = Skeleton->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);
			for(FFloatCurve& Curve : RawCurveData.FloatCurves)
			{
				// Add the names of the curves into the smartname mapping and store off the curve uid which will be saved next time the sequence saves.
				NameMapping->AddOrFindName(Curve.LastObservedName, Curve.CurveUid);
			}
		}
		else
		{
			VerifyCurveNames<FFloatCurve>(Skeleton, USkeleton::AnimCurveMappingName, RawCurveData.FloatCurves);
		}

#if WITH_EDITOR
		VerifyCurveNames<FTransformCurve>(Skeleton, USkeleton::AnimTrackCurveMappingName, RawCurveData.TransformCurves);
#endif
	}
}

float UAnimSequenceBase::GetPlayLength()
{
	return SequenceLength;
}

void UAnimSequenceBase::SortNotifies()
{
	// Sorts using FAnimNotifyEvent::operator<()
	Notifies.Sort();
}

/** 
 * Retrieves AnimNotifies given a StartTime and a DeltaTime.
 * Time will be advanced and support looping if bAllowLooping is true.
 * Supports playing backwards (DeltaTime<0).
 * Returns notifies between StartTime (exclusive) and StartTime+DeltaTime (inclusive)
 */
void UAnimSequenceBase::GetAnimNotifies(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<const FAnimNotifyEvent *> & OutActiveNotifies) const
{
	// Early out if we have no notifies
	if( (Notifies.Num() == 0) || (DeltaTime == 0.f) )
	{
		return;
	}

	bool const bPlayingBackwards = (DeltaTime < 0.f);
	float PreviousPosition = StartTime;
	float CurrentPosition = StartTime;
	float DesiredDeltaMove = DeltaTime;

	do 
	{
		// Disable looping here. Advance to desired position, or beginning / end of animation 
		const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, CurrentPosition, SequenceLength);

		// Verify position assumptions
		checkf(bPlayingBackwards ? (CurrentPosition <= PreviousPosition) : (CurrentPosition >= PreviousPosition), TEXT("in Animsequence %s"), *GetName());
		
		GetAnimNotifiesFromDeltaPositions(PreviousPosition, CurrentPosition, OutActiveNotifies);
	
		// If we've hit the end of the animation, and we're allowed to loop, keep going.
		if( (AdvanceType == ETAA_Finished) &&  bAllowLooping )
		{
			const float ActualDeltaMove = (CurrentPosition - PreviousPosition);
			DesiredDeltaMove -= ActualDeltaMove; 

			PreviousPosition = bPlayingBackwards ? SequenceLength : 0.f;
			CurrentPosition = PreviousPosition;
		}
		else
		{
			break;
		}
	} 
	while( true );
}

/** 
 * Retrieves AnimNotifies between two time positions. ]PreviousPosition, CurrentPosition]
 * Between PreviousPosition (exclusive) and CurrentPosition (inclusive).
 * Supports playing backwards (CurrentPosition<PreviousPosition).
 * Only supports contiguous range, does NOT support looping and wrapping over.
 */
void UAnimSequenceBase::GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition, TArray<const FAnimNotifyEvent *> & OutActiveNotifies) const
{
	// Early out if we have no notifies
	if( (Notifies.Num() == 0) || (PreviousPosition == CurrentPosition) )
	{
		return;
	}

	bool const bPlayingBackwards = (CurrentPosition < PreviousPosition);

	// If playing backwards, flip Min and Max.
	if( bPlayingBackwards )
	{
		for (int32 NotifyIndex=0; NotifyIndex<Notifies.Num(); NotifyIndex++)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = Notifies[NotifyIndex];
			const float NotifyStartTime = AnimNotifyEvent.GetTriggerTime();
			const float NotifyEndTime = AnimNotifyEvent.GetEndTriggerTime();

			if( (NotifyStartTime < PreviousPosition) && (NotifyEndTime >= CurrentPosition) )
			{
				OutActiveNotifies.Add(&AnimNotifyEvent);
			}
		}
	}
	else
	{
		for (int32 NotifyIndex=0; NotifyIndex<Notifies.Num(); NotifyIndex++)
		{
			const FAnimNotifyEvent& AnimNotifyEvent = Notifies[NotifyIndex];
			const float NotifyStartTime = AnimNotifyEvent.GetTriggerTime();
			const float NotifyEndTime = AnimNotifyEvent.GetEndTriggerTime();

			if( (NotifyStartTime <= CurrentPosition) && (NotifyEndTime > PreviousPosition) )
			{
				OutActiveNotifies.Add(&AnimNotifyEvent);
			}
		}
	}
}

void UAnimSequenceBase::TickAssetPlayerInstance(FAnimTickRecord& Instance, class UAnimInstance* InstanceOwner, FAnimAssetTickContext& Context) const
{
	float& CurrentTime = *(Instance.TimeAccumulator);
	float PreviousTime = CurrentTime;
	const float PlayRate = Instance.PlayRateMultiplier * this->RateScale;

	float MoveDelta = 0.f;

	if( Context.IsLeader() )
	{
		const float DeltaTime = Context.GetDeltaTime();
		MoveDelta = PlayRate * DeltaTime;

		Context.SetLeaderDelta(MoveDelta);
		if (MoveDelta != 0.f)
		{
			if (Instance.bCanUseMarkerSync && Context.CanUseMarkerPosition())
			{
				TickByMarkerAsLeader(*Instance.MarkerTickRecord, Context.MarkerTickContext, CurrentTime, PreviousTime, MoveDelta, Instance.bLooping);
			}
			else
			{
				// Advance time
				FAnimationRuntime::AdvanceTime(Instance.bLooping, MoveDelta, CurrentTime, SequenceLength);
				UE_LOG(LogAnimMarkerSync, Log, TEXT("Leader (%s) (normal advance)  - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping (%d) "), *GetName(), PreviousTime, CurrentTime, MoveDelta, Instance.bLooping ? 1 : 0);
			}
		}

		Context.SetAnimationPositionRatio(CurrentTime / SequenceLength);
	}
	else
	{
		// Follow the leader
		if (Instance.bCanUseMarkerSync && Context.CanUseMarkerPosition() && Context.MarkerTickContext.IsMarkerSyncStartValid())
		{
			TickByMarkerAsFollower(*Instance.MarkerTickRecord, Context.MarkerTickContext, CurrentTime, PreviousTime, Context.GetLeaderDelta(), Instance.bLooping);
		}
		else
		{
			CurrentTime = Context.GetAnimationPositionRatio() * SequenceLength;
			UE_LOG(LogAnimMarkerSync, Log, TEXT("Follower (%s) (normal advance) - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping (%d) "), *GetName(), PreviousTime, CurrentTime, MoveDelta, Instance.bLooping ? 1 : 0);
		}


		//@TODO: NOTIFIES: Calculate AdvanceType based on what the new delta time is

		if( CurrentTime != PreviousTime )
		{
			// Figure out delta time 
			MoveDelta = CurrentTime - PreviousTime;
			// if we went against play rate, then loop around.
			if( (MoveDelta * PlayRate) < 0.f )
			{
				MoveDelta += FMath::Sign<float>(PlayRate) * SequenceLength;
			}
		}
	}

	OnAssetPlayerTickedInternal(Context, PreviousTime, MoveDelta, Instance, InstanceOwner);
}

void UAnimSequenceBase::TickByMarkerAsFollower(FMarkerTickRecord &Instance, FMarkerTickContext &MarkerContext, float& CurrentTime, float& OutPreviousTime, const float MoveDelta, const bool bLooping) const
{
	if (!Instance.IsValid())
	{
		GetMarkerIndicesForPosition(MarkerContext.GetMarkerSyncStartPosition(), bLooping, Instance.PreviousMarker, Instance.NextMarker, CurrentTime);
	}

	OutPreviousTime = CurrentTime;

	AdvanceMarkerPhaseAsFollower(MarkerContext, MoveDelta, bLooping, CurrentTime, Instance.PreviousMarker, Instance.NextMarker);
	UE_LOG(LogAnimMarkerSync, Log, TEXT("Follower (%s) - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping(%d) "), *GetName(), OutPreviousTime, CurrentTime, MoveDelta, bLooping ? 1 : 0);
}

void UAnimSequenceBase::TickByMarkerAsLeader(FMarkerTickRecord& Instance, FMarkerTickContext& MarkerContext, float& CurrentTime, float& OutPreviousTime, const float MoveDelta, const bool bLooping) const
{
	if (!Instance.IsValid())
	{
		GetMarkerIndicesForTime(CurrentTime, bLooping, MarkerContext.GetValidMarkerNames(), Instance.PreviousMarker, Instance.NextMarker);
	}

	MarkerContext.SetMarkerSyncStartPosition(GetMarkerSyncPositionfromMarkerIndicies(Instance.PreviousMarker.MarkerIndex, Instance.NextMarker.MarkerIndex, CurrentTime));

	OutPreviousTime = CurrentTime;

	AdvanceMarkerPhaseAsLeader(bLooping, MoveDelta, MarkerContext.GetValidMarkerNames(), CurrentTime, Instance.PreviousMarker, Instance.NextMarker, MarkerContext.MarkersPassedThisTick);

	MarkerContext.SetMarkerSyncEndPosition(GetMarkerSyncPositionfromMarkerIndicies(Instance.PreviousMarker.MarkerIndex, Instance.NextMarker.MarkerIndex, CurrentTime));

	UE_LOG(LogAnimMarkerSync, Log, TEXT("Leader (%s) - PreviousTime (%0.2f), CurrentTime (%0.2f), MoveDelta (%0.2f), Looping(%d) "), *GetName(), OutPreviousTime, CurrentTime, MoveDelta, bLooping ? 1 : 0);
}

void UAnimSequenceBase::RefreshCacheData()
{
#if WITH_EDITOR
	SortNotifies();

	for (int32 TrackIndex=0; TrackIndex<AnimNotifyTracks.Num(); ++TrackIndex)
	{
		AnimNotifyTracks[TrackIndex].Notifies.Empty();
	}

	for (int32 NotifyIndex = 0; NotifyIndex<Notifies.Num(); ++NotifyIndex)
	{
		int32 TrackIndex = Notifies[NotifyIndex].TrackIndex;
		if (AnimNotifyTracks.IsValidIndex(TrackIndex))
		{
			AnimNotifyTracks[TrackIndex].Notifies.Add(&Notifies[NotifyIndex]);
		}
		else
		{
			// this notifyindex isn't valid, delete
			// this should not happen, but if it doesn, find best place to add
			ensureMsgf(0, TEXT("AnimNotifyTrack: Wrong indices found"));
			AnimNotifyTracks[0].Notifies.Add(&Notifies[NotifyIndex]);
		}
	}

	// notification broadcast
	OnNotifyChanged.Broadcast();
#endif //WITH_EDITOR
}

#if WITH_EDITOR
void UAnimSequenceBase::InitializeNotifyTrack()
{
	if ( AnimNotifyTracks.Num() == 0 ) 
	{
		AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("1"), FLinearColor::White ));
	}
}

int32 UAnimSequenceBase::GetNumberOfFrames() const
{
	return (SequenceLength/0.033f);
}

int32 UAnimSequenceBase::GetFrameAtTime(const float Time) const
{
	const float Frac = Time / SequenceLength;
	return FMath::FloorToInt(Frac * GetNumberOfFrames());
}

float UAnimSequenceBase::GetTimeAtFrame(const int32 Frame) const
{
	const float FrameTime = SequenceLength / GetNumberOfFrames();
	return FrameTime * Frame;
}

void UAnimSequenceBase::RegisterOnNotifyChanged(const FOnNotifyChanged& Delegate)
{
	OnNotifyChanged.Add(Delegate);
}
void UAnimSequenceBase::UnregisterOnNotifyChanged(void* Unregister)
{
	OnNotifyChanged.RemoveAll(Unregister);
}

void UAnimSequenceBase::ClampNotifiesAtEndOfSequence()
{
	const float NotifyClampTime = SequenceLength - 0.01f; //Slight offset so that notify is still draggable
	for(int i = 0; i < Notifies.Num(); ++ i)
	{
		if(Notifies[i].GetTime() >= SequenceLength)
		{
			Notifies[i].SetTime(NotifyClampTime);
			Notifies[i].TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::OffsetBefore);
		}
	}
}

EAnimEventTriggerOffsets::Type UAnimSequenceBase::CalculateOffsetForNotify(float NotifyDisplayTime) const
{
	if(NotifyDisplayTime == 0.f)
	{
		return EAnimEventTriggerOffsets::OffsetAfter;
	}
	else if(NotifyDisplayTime == SequenceLength)
	{
		return EAnimEventTriggerOffsets::OffsetBefore;
	}
	return EAnimEventTriggerOffsets::NoOffset;
}

void UAnimSequenceBase::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
	if(Notifies.Num() > 0)
	{
		FString NotifyList;

		// add notifies to 
		for(auto Iter=Notifies.CreateConstIterator(); Iter; ++Iter)
		{
			// only add if not BP anim notify since they're handled separate
			if(Iter->IsBlueprintNotify() == false)
			{
				NotifyList += FString::Printf(TEXT("%s%s"), *Iter->NotifyName.ToString(), *USkeleton::AnimNotifyTagDelimiter);
			}
		}
		
		if(NotifyList.Len() > 0)
		{
			OutTags.Add(FAssetRegistryTag(USkeleton::AnimNotifyTag, NotifyList, FAssetRegistryTag::TT_Hidden));
		}
	}

	// Add curve IDs to a tag list, or a blank tag if we have no curves.
	// The blank list is necessary when we attempt to delete a curve so
	// an old asset can be detected from its asset data so we load as few
	// as possible.
	FString CurveIdList;

	for(const FFloatCurve& Curve : RawCurveData.FloatCurves)
	{
		CurveIdList += FString::Printf(TEXT("%u%s"), Curve.CurveUid, *USkeleton::CurveTagDelimiter);
	}
	OutTags.Add(FAssetRegistryTag(USkeleton::CurveTag, CurveIdList, FAssetRegistryTag::TT_Hidden));
}

uint8* UAnimSequenceBase::FindNotifyPropertyData(int32 NotifyIndex, UArrayProperty*& ArrayProperty)
{
	// initialize to NULL
	ArrayProperty = NULL;

	if(Notifies.IsValidIndex(NotifyIndex))
	{
		return FindArrayProperty(TEXT("Notifies"), ArrayProperty, NotifyIndex);
	}
	return NULL;
}

uint8* UAnimSequenceBase::FindArrayProperty(const TCHAR* PropName, UArrayProperty*& ArrayProperty, int32 ArrayIndex)
{
	// find Notifies property start point
	UProperty* Property = FindField<UProperty>(GetClass(), PropName);

	// found it and if it is array
	if (Property && Property->IsA(UArrayProperty::StaticClass()))
	{
		// find Property Value from UObject we got
		uint8* PropertyValue = Property->ContainerPtrToValuePtr<uint8>(this);

		// it is array, so now get ArrayHelper and find the raw ptr of the data
		ArrayProperty = CastChecked<UArrayProperty>(Property);
		FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyValue);

		if (ArrayProperty->Inner && ArrayIndex < ArrayHelper.Num())
		{
			//Get property data based on selected index
			return ArrayHelper.GetRawPtr(ArrayIndex);
		}
	}
	return NULL;
}
#endif	//WITH_EDITOR


/** Add curve data to Instance at the time of CurrentTime **/
void UAnimSequenceBase::EvaluateCurveData(FBlendedCurve& OutCurve, float CurrentTime ) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimSeq_EvalCurveData);
	RawCurveData.EvaluateCurveData(OutCurve, CurrentTime);
}

void UAnimSequenceBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.ArIsSaving && Ar.UE4Ver() >= VER_UE4_SKELETON_ADD_SMARTNAMES)
	{
		if(USkeleton* Skeleton = GetSkeleton())
		{
			FSmartNameMapping* Mapping = GetSkeleton()->SmartNames.GetContainer(USkeleton::AnimCurveMappingName);
			check(Mapping); // Should always exist
			RawCurveData.UpdateLastObservedNames(Mapping);
		}
	}

#if WITH_EDITORONLY_DATA
	if(Ar.ArIsSaving && Ar.UE4Ver() >= VER_UE4_ANIMATION_ADD_TRACKCURVES)
	{
		if(USkeleton* Skeleton = GetSkeleton())
		{
			// we don't add track curve container unless it has been edited. 
			if ( RawCurveData.TransformCurves.Num() > 0 )
			{
				FSmartNameMapping* Mapping = GetSkeleton()->SmartNames.GetContainer(USkeleton::AnimTrackCurveMappingName);
				// this name might not exists because it's only available if you edit animation
				if (Mapping)
				{
					RawCurveData.UpdateLastObservedNames(Mapping, FRawCurveTracks::TransformType);
				}
			}
		}
	}
#endif //WITH_EDITORONLY_DATA

	RawCurveData.Serialize(Ar);
}

void UAnimSequenceBase::OnAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, class UAnimInstance* InstanceOwner) const
{
	if (Context.ShouldGenerateNotifies())
	{
		// Harvest and record notifies
		TArray<const FAnimNotifyEvent*> AnimNotifies;
		GetAnimNotifies(PreviousTime, MoveDelta, Instance.bLooping, AnimNotifies);
		InstanceOwner->AddAnimNotifies(AnimNotifies, Instance.EffectiveBlendWeight);
	}
}
