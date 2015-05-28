// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AnimationUtils.h"
#include "AnimationRuntime.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimInstance.h"

#define NOTIFY_TRIGGER_OFFSET KINDA_SMALL_NUMBER;

float GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::Type OffsetType)
{
	switch (OffsetType)
	{
	case EAnimEventTriggerOffsets::OffsetBefore:
		{
			return -NOTIFY_TRIGGER_OFFSET;
			break;
		}
	case EAnimEventTriggerOffsets::OffsetAfter:
		{
			return NOTIFY_TRIGGER_OFFSET;
			break;
		}
	case EAnimEventTriggerOffsets::NoOffset:
		{
			return 0.f;
			break;
		}
	default:
		{
			check(false); // Unknown value supplied for OffsetType
			break;
		}
	}
	return 0.f;
}

/////////////////////////////////////////////////////
// FAnimNotifyEvent

void FAnimNotifyEvent::RefreshTriggerOffset(EAnimEventTriggerOffsets::Type PredictedOffsetType)
{
	if(PredictedOffsetType == EAnimEventTriggerOffsets::NoOffset || TriggerTimeOffset == 0.f)
	{
		TriggerTimeOffset = GetTriggerTimeOffsetForType(PredictedOffsetType);
	}
}

void FAnimNotifyEvent::RefreshEndTriggerOffset( EAnimEventTriggerOffsets::Type PredictedOffsetType )
{
	if(PredictedOffsetType == EAnimEventTriggerOffsets::NoOffset || EndTriggerTimeOffset == 0.f)
	{
		EndTriggerTimeOffset = GetTriggerTimeOffsetForType(PredictedOffsetType);
	}
}

float FAnimNotifyEvent::GetTriggerTime() const
{
	return GetTime() + TriggerTimeOffset;
}

float FAnimNotifyEvent::GetEndTriggerTime() const
{
	return GetTriggerTime() + GetDuration() + EndTriggerTimeOffset;
}

float FAnimNotifyEvent::GetDuration() const
{
	return NotifyStateClass ? EndLink.GetTime() - GetTime() : 0.0f;
}

void FAnimNotifyEvent::SetDuration(float NewDuration)
{
	Duration = NewDuration;
	EndLink.SetTime(GetTime() + Duration);
}

void FAnimNotifyEvent::SetTime(float NewTime, EAnimLinkMethod::Type ReferenceFrame /*= EAnimLinkMethod::Absolute*/)
{
	FAnimLinkableElement::SetTime(NewTime, ReferenceFrame);
	SetDuration(Duration);
}

/////////////////////////////////////////////////////
// FFloatCurve

void FAnimCurveBase::SetCurveTypeFlag(EAnimCurveFlags InFlag, bool bValue)
{
	if (bValue)
	{
		CurveTypeFlags |= InFlag;
	}
	else
	{
		CurveTypeFlags &= ~InFlag;
	}
}

void FAnimCurveBase::ToggleCurveTypeFlag(EAnimCurveFlags InFlag)
{
	bool Current = GetCurveTypeFlag(InFlag);
	SetCurveTypeFlag(InFlag, !Current);
}

bool FAnimCurveBase::GetCurveTypeFlag(EAnimCurveFlags InFlag) const
{
	return (CurveTypeFlags & InFlag) != 0;
}


void FAnimCurveBase::SetCurveTypeFlags(int32 NewCurveTypeFlags)
{
	CurveTypeFlags = NewCurveTypeFlags;
}

int32 FAnimCurveBase::GetCurveTypeFlags() const
{
	return CurveTypeFlags;
}

////////////////////////////////////////////////////
//  FFloatCurve

// we don't want to have = operator. This only copies curves, but leaving naming and everything else intact. 
void FFloatCurve::CopyCurve(FFloatCurve& SourceCurve)
{
	FloatCurve = SourceCurve.FloatCurve;
}

float FFloatCurve::Evaluate(float CurrentTime, float BlendWeight) const
{
	return FloatCurve.Eval(CurrentTime)*BlendWeight;
}

void FFloatCurve::UpdateOrAddKey(float NewKey, float CurrentTime)
{
	FloatCurve.UpdateOrAddKey(CurrentTime, NewKey);
}

void FFloatCurve::Resize(float NewLength)
{
	FloatCurve.ResizeTimeRange(0, NewLength);
}
////////////////////////////////////////////////////
//  FVectorCurve

// we don't want to have = operator. This only copies curves, but leaving naming and everything else intact. 
void FVectorCurve::CopyCurve(FVectorCurve& SourceCurve)
{
	FMemory::Memcpy(FloatCurves, SourceCurve.FloatCurves);
}

FVector FVectorCurve::Evaluate(float CurrentTime, float BlendWeight) const
{
	FVector Value;

	Value.X = FloatCurves[X].Eval(CurrentTime)*BlendWeight;
	Value.Y = FloatCurves[Y].Eval(CurrentTime)*BlendWeight;
	Value.Z = FloatCurves[Z].Eval(CurrentTime)*BlendWeight;

	return Value;
}

void FVectorCurve::UpdateOrAddKey(const FVector& NewKey, float CurrentTime)
{
	FloatCurves[X].UpdateOrAddKey(CurrentTime, NewKey.X);
	FloatCurves[Y].UpdateOrAddKey(CurrentTime, NewKey.Y);
	FloatCurves[Z].UpdateOrAddKey(CurrentTime, NewKey.Z);
}

void FVectorCurve::Resize(float NewLength)
{
	FloatCurves[X].ResizeTimeRange(0, NewLength);
	FloatCurves[Y].ResizeTimeRange(0, NewLength);
	FloatCurves[Z].ResizeTimeRange(0, NewLength);
}
////////////////////////////////////////////////////
//  FTransformCurve

// we don't want to have = operator. This only copies curves, but leaving naming and everything else intact. 
void FTransformCurve::CopyCurve(FTransformCurve& SourceCurve)
{
	TranslationCurve.CopyCurve(SourceCurve.TranslationCurve);
	RotationCurve.CopyCurve(SourceCurve.RotationCurve);
	ScaleCurve.CopyCurve(SourceCurve.ScaleCurve);
}

FTransform FTransformCurve::Evaluate(float CurrentTime, float BlendWeight) const
{
	FTransform Value;
	Value.SetTranslation(TranslationCurve.Evaluate(CurrentTime, BlendWeight));
	if (ScaleCurve.DoesContainKey())
	{
		Value.SetScale3D(ScaleCurve.Evaluate(CurrentTime, BlendWeight));
	}
	else
	{
		Value.SetScale3D(FVector(1.f));
	}

	// blend rotation float curve
	FVector RotationAsVector = RotationCurve.Evaluate(CurrentTime, BlendWeight);
	// pitch, yaw, roll order - please check AddKey function
	FRotator Rotator(RotationAsVector.Y, RotationAsVector.Z, RotationAsVector.X);
	Value.SetRotation(FQuat(Rotator));

	return Value;
}

void FTransformCurve::UpdateOrAddKey(const FTransform& NewKey, float CurrentTime)
{
	TranslationCurve.UpdateOrAddKey(NewKey.GetTranslation(), CurrentTime);
	// pitch, yaw, roll order - please check Evaluate function
	FVector RotationAsVector;
	FRotator Rotator = NewKey.GetRotation().Rotator();
	RotationAsVector.X = Rotator.Roll;
	RotationAsVector.Y = Rotator.Pitch;
	RotationAsVector.Z = Rotator.Yaw;

	RotationCurve.UpdateOrAddKey(RotationAsVector, CurrentTime);
	ScaleCurve.UpdateOrAddKey(NewKey.GetScale3D(), CurrentTime);
}

void FTransformCurve::Resize(float NewLength)
{
	TranslationCurve.Resize(NewLength);
	RotationCurve.Resize(NewLength);
	ScaleCurve.Resize(NewLength);
}

/////////////////////////////////////////////////////
// FRawCurveTracks

void FRawCurveTracks::EvaluateCurveData(class UAnimInstance* Instance, float CurrentTime, float BlendWeight ) const
{
	// evaluate the curve data at the CurrentTime and add to Instance
	for(auto CurveIter = FloatCurves.CreateConstIterator(); CurveIter; ++CurveIter)
	{
		const FFloatCurve& Curve = *CurveIter;

		Instance->AddCurveValue(Curve.CurveUid, Curve.Evaluate(CurrentTime, BlendWeight), Curve.GetCurveTypeFlags());
	}
}

#if WITH_EDITOR
/**
 * Since we don't care about blending, we just change this decoration to OutCurves
 * @TODO : Fix this if we're saving vectorcurves and blending
 */
void FRawCurveTracks::EvaluateTransformCurveData(USkeleton * Skeleton, TMap<FName, FTransform>&OutCurves, float CurrentTime, float BlendWeight) const
{
	check (Skeleton);
	// evaluate the curve data at the CurrentTime and add to Instance
	for(auto CurveIter = TransformCurves.CreateConstIterator(); CurveIter; ++CurveIter)
	{
		const FTransformCurve& Curve = *CurveIter;

		// if disabled, do not handle
		if (Curve.GetCurveTypeFlag(ACF_Disabled))
		{
			continue;
		}

		FSmartNameMapping* NameMapping = Skeleton->SmartNames.GetContainer(USkeleton::AnimTrackCurveMappingName);

		// Add or retrieve curve
		FName CurveName;
		
		// make sure it was added
		if (ensure (NameMapping->GetName(Curve.CurveUid, CurveName)))
		{
			// note we're not checking Curve.GetCurveTypeFlags() yet
			FTransform & Value = OutCurves.FindOrAdd(CurveName);
			Value = Curve.Evaluate(CurrentTime, BlendWeight);
		}
	}
}
#endif
FAnimCurveBase * FRawCurveTracks::GetCurveData(USkeleton::AnimCurveUID Uid, ESupportedCurveType SupportedCurveType /*= FloatType*/)
{
	switch (SupportedCurveType)
	{
#if WITH_EDITOR
	case VectorType:
		return GetCurveDataImpl<FVectorCurve>(VectorCurves, Uid);
	case TransformType:
		return GetCurveDataImpl<FTransformCurve>(TransformCurves, Uid);
#endif // WITH_EDITOR
	case FloatType:
	default:
		return GetCurveDataImpl<FFloatCurve>(FloatCurves, Uid);
	}
}

bool FRawCurveTracks::DeleteCurveData(USkeleton::AnimCurveUID Uid, ESupportedCurveType SupportedCurveType /*= FloatType*/)
{
	switch(SupportedCurveType)
	{
#if WITH_EDITOR
	case VectorType:
		return DeleteCurveDataImpl<FVectorCurve>(VectorCurves, Uid);
	case TransformType:
		return DeleteCurveDataImpl<FTransformCurve>(TransformCurves, Uid);
#endif // WITH_EDITOR
	case FloatType:
	default:
		return DeleteCurveDataImpl<FFloatCurve>(FloatCurves, Uid);
	}
}

bool FRawCurveTracks::AddCurveData(USkeleton::AnimCurveUID Uid, int32 CurveFlags /*= ACF_DefaultCurve*/, ESupportedCurveType SupportedCurveType /*= FloatType*/)
{
	switch(SupportedCurveType)
	{
#if WITH_EDITOR
	case VectorType:
		return AddCurveDataImpl<FVectorCurve>(VectorCurves, Uid, CurveFlags);
	case TransformType:
		return AddCurveDataImpl<FTransformCurve>(TransformCurves, Uid, CurveFlags);
#endif // WITH_EDITOR
	case FloatType:
	default:
		return AddCurveDataImpl<FFloatCurve>(FloatCurves, Uid, CurveFlags);
	}
}

void FRawCurveTracks::Resize(float TotalLength)
{
	for (auto& Curve: FloatCurves)
	{
		Curve.Resize(TotalLength);
	}

#if WITH_EDITORONLY_DATA
	for(auto& Curve: VectorCurves)
	{
		Curve.Resize(TotalLength);
	}

	for(auto& Curve: TransformCurves)
	{
		Curve.Resize(TotalLength);
	}
#endif
}

void FRawCurveTracks::Serialize(FArchive& Ar)
{
	// @TODO: If we're about to serialize vector curve, add here
	if(Ar.UE4Ver() >= VER_UE4_SKELETON_ADD_SMARTNAMES)
	{
		for(FFloatCurve& Curve : FloatCurves)
		{
			Curve.Serialize(Ar);
		}
	}
#if WITH_EDITORONLY_DATA
	if( !Ar.IsCooking() )
	{
		if( Ar.UE4Ver() >= VER_UE4_ANIMATION_ADD_TRACKCURVES )
		{
			for( FTransformCurve& Curve : TransformCurves )
			{
				Curve.Serialize( Ar );
			}

		}
	}
#endif // WITH_EDITORONLY_DATA
}

void FRawCurveTracks::UpdateLastObservedNames(FSmartNameMapping* NameMapping, ESupportedCurveType SupportedCurveType /*= FloatType*/)
{
	switch(SupportedCurveType)
	{
#if WITH_EDITOR
	case VectorType:
		UpdateLastObservedNamesImpl<FVectorCurve>(VectorCurves, NameMapping);
		break;
	case TransformType:
		UpdateLastObservedNamesImpl<FTransformCurve>(TransformCurves, NameMapping);
		break;
#endif // WITH_EDITOR
	case FloatType:
	default:
		UpdateLastObservedNamesImpl<FFloatCurve>(FloatCurves, NameMapping);
	}
}

bool FRawCurveTracks::DuplicateCurveData(USkeleton::AnimCurveUID ToCopyUid, USkeleton::AnimCurveUID NewUid, ESupportedCurveType SupportedCurveType /*= FloatType*/)
{
	switch(SupportedCurveType)
	{
#if WITH_EDITOR
	case VectorType:
		return DuplicateCurveDataImpl<FVectorCurve>(VectorCurves, ToCopyUid, NewUid);
	case TransformType:
		return DuplicateCurveDataImpl<FTransformCurve>(TransformCurves, ToCopyUid, NewUid);
#endif // WITH_EDITOR
	case FloatType:
	default:
		return DuplicateCurveDataImpl<FFloatCurve>(FloatCurves, ToCopyUid, NewUid);
	}
}

///////////////////////////////////
// @TODO: REFACTOR THIS IF WE'RE SERIALIZING VECTOR CURVES
//
// implementation template functions to accomodate FloatCurve and VectorCurve
// for now vector curve isn't used in run-time, so it's useless outside of editor
// so just to reduce cost of run-time, functionality is split. 
// this split worries me a bit because if the name conflict happens this will break down w.r.t. smart naming
// currently vector curve is not saved and not evaluated, so it will be okay since the name doesn't matter much, 
// but this has to be refactored once we'd like to move onto serialize
///////////////////////////////////
template <typename DataType>
DataType * FRawCurveTracks::GetCurveDataImpl(TArray<DataType> & Curves, USkeleton::AnimCurveUID Uid)
{
	for(DataType& Curve : Curves)
	{
		if(Curve.CurveUid == Uid)
		{
			return &Curve;
		}
	}

	return NULL;
}

template <typename DataType>
bool FRawCurveTracks::DeleteCurveDataImpl(TArray<DataType> & Curves, USkeleton::AnimCurveUID Uid)
{
	for(int32 Idx = 0; Idx < Curves.Num(); ++Idx)
	{
		if(Curves[Idx].CurveUid == Uid)
		{
			Curves.RemoveAt(Idx);
			return true;
		}
	}

	return false;
}

template <typename DataType>
bool FRawCurveTracks::AddCurveDataImpl(TArray<DataType> & Curves, USkeleton::AnimCurveUID Uid, int32 CurveFlags)
{
	if(GetCurveDataImpl<DataType>(Curves, Uid) == NULL)
	{
		Curves.Add(DataType(Uid, CurveFlags));
		return true;
	}
	return false;
}

template <typename DataType>
void FRawCurveTracks::UpdateLastObservedNamesImpl(TArray<DataType> & Curves, FSmartNameMapping* NameMapping)
{
	if(NameMapping)
	{
		for(DataType& Curve : Curves)
		{
			NameMapping->GetName(Curve.CurveUid, Curve.LastObservedName);
		}
	}
}

template <typename DataType>
bool FRawCurveTracks::DuplicateCurveDataImpl(TArray<DataType> & Curves, USkeleton::AnimCurveUID ToCopyUid, USkeleton::AnimCurveUID NewUid)
{
	DataType* ExistingCurve = GetCurveDataImpl<DataType>(Curves, ToCopyUid);
	if(ExistingCurve && GetCurveDataImpl<DataType>(Curves, NewUid) == NULL)
	{
		// Add the curve to the track and set its data to the existing curve
		Curves.Add(DataType(NewUid, ExistingCurve->GetCurveTypeFlags()));
		Curves.Last().CopyCurve(*ExistingCurve);

		return true;
	}
	return false;
}

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
	UpdateAnimNotifyTrackCache();
#endif

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

void UAnimSequenceBase::TickAssetPlayerInstance(const FAnimTickRecord& Instance, class UAnimInstance* InstanceOwner, FAnimAssetTickContext& Context) const
{
	float& CurrentTime = *(Instance.TimeAccumulator);
	const float PreviousTime = CurrentTime;
	const float PlayRate = Instance.PlayRateMultiplier * this->RateScale;

	float MoveDelta = 0.f;

	if( Context.IsLeader() )
	{
		const float DeltaTime = Context.GetDeltaTime();
		MoveDelta = PlayRate * DeltaTime;

		if( MoveDelta != 0.f )
		{
			// Advance time
			FAnimationRuntime::AdvanceTime(Instance.bLooping, MoveDelta, CurrentTime, SequenceLength);
		}

		Context.SetSyncPoint(CurrentTime / SequenceLength);
	}
	else
	{
		// Follow the leader
		CurrentTime = Context.GetSyncPoint() * SequenceLength;
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

	// Evaluate Curve data now - even if time did not move, we still need to return curve if it exists
	EvaluateCurveData(InstanceOwner, CurrentTime, Instance.EffectiveBlendWeight);
}

#if WITH_EDITOR

void UAnimSequenceBase::UpdateAnimNotifyTrackCache()
{
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
			ensureMsg(0, TEXT("AnimNotifyTrack: Wrong indices found"));
			AnimNotifyTracks[0].Notifies.Add(&Notifies[NotifyIndex]);
		}
	}

	// notification broadcast
	OnNotifyChanged.Broadcast();
}

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
		// find Notifies property start point
		UProperty* Property = FindField<UProperty>(GetClass(), TEXT("Notifies"));

		// found it and if it is array
		if(Property && Property->IsA(UArrayProperty::StaticClass()))
		{
			// find Property Value from UObject we got
			uint8* PropertyValue = Property->ContainerPtrToValuePtr<uint8>(this);

			// it is array, so now get ArrayHelper and find the raw ptr of the data
			ArrayProperty = CastChecked<UArrayProperty>(Property);
			FScriptArrayHelper ArrayHelper(ArrayProperty, PropertyValue);

			if(ArrayProperty->Inner && NotifyIndex < ArrayHelper.Num())
			{
				//Get property data based on selected index
				return ArrayHelper.GetRawPtr(NotifyIndex);
			}
		}
	}
	return NULL;
}

#endif	//WITH_EDITOR


/** Add curve data to Instance at the time of CurrentTime **/
void UAnimSequenceBase::EvaluateCurveData(class UAnimInstance* Instance, float CurrentTime, float BlendWeight ) const
{
	// if we have compression, this should change
	// this is raw data evaluation
	RawCurveData.EvaluateCurveData(Instance, CurrentTime, BlendWeight);
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
			// we don't add track curve container unless it has been editied. 
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
