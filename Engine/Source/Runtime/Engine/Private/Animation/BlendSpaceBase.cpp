// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BlendSpaceBase.cpp: Base class for blend space objects
=============================================================================*/ 

#include "EnginePrivate.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstance.h"
#include "Animation/BlendSpaceBase.h"

// global variable that's used in editor when you change the property only
// I can't make this member since I  need to change this in const function
bool bNeedReinitializeFilter = false;

UBlendSpaceBase::UBlendSpaceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

void UBlendSpaceBase::PostLoad()
{
	Super::PostLoad();

	ValidateSampleData();

	InitializePerBoneBlend();
}

void UBlendSpaceBase::InitializePerBoneBlend()
{
	const USkeleton* MySkeleton = GetSkeleton();
	// also initialize perbone interpolation data
	for (int32 Id=0; Id<PerBoneBlend.Num(); ++Id) 
	{
		PerBoneBlend[Id].Initialize(MySkeleton);
	}

	struct FCompareFPerBoneInterpolation
	{
		FORCEINLINE bool operator()( const FPerBoneInterpolation& A, const FPerBoneInterpolation& B ) const
		{
			return A.BoneReference.BoneIndex > B.BoneReference.BoneIndex;
		}
	};

	// Sort this by bigger to smaller, then we don't have to worry about checking the best parent
	PerBoneBlend.Sort( FCompareFPerBoneInterpolation() );
}

void UBlendSpaceBase::InitializeFilter(FBlendFilter * Filter) const
{
	if (Filter)
	{
		Filter->FilterPerAxis[0].Initialize(InterpolationParam[0].InterpolationTime, InterpolationParam[0].InterpolationType);
		Filter->FilterPerAxis[1].Initialize(InterpolationParam[1].InterpolationTime, InterpolationParam[1].InterpolationType);
		Filter->FilterPerAxis[2].Initialize(InterpolationParam[2].InterpolationTime, InterpolationParam[2].InterpolationType);
	}
}

FVector UBlendSpaceBase::FilterInput(FBlendFilter * Filter, const FVector& BlendInput, float DeltaTime) const
{
	if (bNeedReinitializeFilter)
	{
		InitializeFilter(Filter);
		bNeedReinitializeFilter = false;
	}

	FVector FilteredBlendInput;
	FilteredBlendInput.X = Filter->FilterPerAxis[0].GetFilteredData(BlendInput.X, DeltaTime);
	FilteredBlendInput.Y = Filter->FilterPerAxis[1].GetFilteredData(BlendInput.Y, DeltaTime);
	FilteredBlendInput.Z = Filter->FilterPerAxis[2].GetFilteredData(BlendInput.Z, DeltaTime);
	return FilteredBlendInput;
}

void UBlendSpaceBase::TickAssetPlayerInstance(const FAnimTickRecord& Instance, class UAnimInstance* InstanceOwner, FAnimAssetTickContext& Context) const
{
	const float DeltaTime = Context.GetDeltaTime();
	float MoveDelta = Instance.PlayRateMultiplier * DeltaTime;

	// this happens even if MoveDelta == 0.f. This still should happen if it is being interpolated
	// since we allow setting position of blendspace, we can't ignore MoveDelta == 0.f
	// also now we don't have to worry about not following if DeltaTime = 0.f
	{
		// first filter input using blend filter
		const FVector BlendInput = FilterInput(Instance.BlendFilter, Instance.BlendSpacePosition, DeltaTime);
		EBlendSpaceAxis AxisToScale = GetAxisToScale();
		if (AxisToScale != BSA_None)
		{
			float FilterMultiplier = 1.f;
			// first use multiplier using new blendinput
			// new filtered input is going to be used for sampling animation
			// so we'll need to change playrate if you'd like to not slide foot
			if ( !Instance.BlendSpacePosition.Equals(BlendInput) )  
			{
				// apply speed change if you want, 
				if (AxisToScale == BSA_X)
				{
					if (BlendInput.X != 0.f)
					{
						FilterMultiplier = Instance.BlendSpacePosition.X / BlendInput.X;
					}
				}
				else if (AxisToScale == BSA_Y)
				{
					if (BlendInput.Y != 0.f)
					{
						FilterMultiplier = Instance.BlendSpacePosition.Y / BlendInput.Y;
					}
				}
			}

			// now find if clamped input is different
			// if different, then apply scale to fit in
			FVector ClampedInput = ClampBlendInput(BlendInput);
			if ( !ClampedInput.Equals(BlendInput) ) 
			{
				// apply speed change if you want, 
				if (AxisToScale == BSA_X)
				{
					if (ClampedInput.X != 0.f)
					{
						FilterMultiplier *= BlendInput.X / ClampedInput.X;
					}
				}
				else if (AxisToScale == BSA_Y)
				{
					if (ClampedInput.Y != 0.f)
					{
						FilterMultiplier *= BlendInput.Y / ClampedInput.Y;
					}
				}
			}


			MoveDelta *= FilterMultiplier;
			UE_LOG(LogAnimation, Log, TEXT("BlendSpace(%s) - BlendInput(%s) : FilteredBlendInput(%s), FilterMultiplier(%0.2f)"), *GetName(), *Instance.BlendSpacePosition.ToString(), *BlendInput.ToString(), FilterMultiplier );
		}

		check (Instance.BlendSampleDataCache);

		// For Target weight interpolation, we'll need to save old data, and interpolate to new data
		TArray<FBlendSampleData> OldSampleDataList, NewSampleDataList;
		OldSampleDataList.Append(*Instance.BlendSampleDataCache);

		// get sample data based on new input
		// consolidate all samples and sort them, so that we can handle from biggest weight to smallest
		Instance.BlendSampleDataCache->Empty();
		// new sample data that will be used for evaluation
		TArray<FBlendSampleData> & SampleDataList = *Instance.BlendSampleDataCache;

		// get sample data from blendspace
		if (GetSamplesFromBlendInput(BlendInput, NewSampleDataList))
		{
			float NewAnimLength=0;

			// if target weight interpolation is set
			if (TargetWeightInterpolationSpeedPerSec > 0.f)
			{
				UE_LOG(LogAnimation, Verbose, TEXT("Target Weight Interpolation: Target Samples "));
				// recalculate AnimLength based on weight of target animations - this is used for scaling animation later (change speed)
				float PreInterpAnimLength = GetAnimationLengthFromSampleData(NewSampleDataList);
				UE_LOG(LogAnimation, Verbose, TEXT("BlendSpace(%s) - BlendInput(%s) : PreAnimLength(%0.5f) "), *GetName(), *BlendInput.ToString(), PreInterpAnimLength);

				// target weight interpolation
				if (InterpolateWeightOfSampleData(DeltaTime, OldSampleDataList, NewSampleDataList, SampleDataList))
				{
					// now I need to normalize
					NormalizeSampleDataWeight(SampleDataList);
				}
				else
				{
					// if interpolation failed, just copy new sample data tto sample data
					SampleDataList = NewSampleDataList;
				}

				// recalculate AnimLength based on weight of animations
				UE_LOG(LogAnimation, Verbose, TEXT("Target Weight Interpolation: Interp Samples "));
				NewAnimLength = GetAnimationLengthFromSampleData(SampleDataList);
				// now scale the animation
				if (NewAnimLength > 0.f)
				{
					MoveDelta *= PreInterpAnimLength/NewAnimLength;
				}
			}
			else
			{
				// when there is no target weight interpolation, just copy new to target
				SampleDataList.Append(NewSampleDataList);
				NewAnimLength = GetAnimationLengthFromSampleData(SampleDataList);
			}

			float& NormalizedCurrentTime = *(Instance.TimeAccumulator);
			const float NormalizedPreviousTime = NormalizedCurrentTime;

			if (Context.IsLeader())
			{
				// advance current time - blend spaces hold normalized time as when dealing with changing anim length it would be possible to go backwards
				UE_LOG(LogAnimation, Verbose, TEXT("BlendSpace(%s) - BlendInput(%s) : AnimLength(%0.5f) "), *GetName(), *BlendInput.ToString(), NewAnimLength);

				// Advance time using current/new anim length
				float CurrentTime = NormalizedCurrentTime * NewAnimLength;
				FAnimationRuntime::AdvanceTime(Instance.bLooping, MoveDelta, /*inout*/ CurrentTime, NewAnimLength);
				NormalizedCurrentTime = NewAnimLength ? (CurrentTime / NewAnimLength) : 0.0f;

				Context.SetSyncPoint(NormalizedCurrentTime);
			}
			else
			{
				NormalizedCurrentTime = Context.GetSyncPoint();
			}

			// generate notifies and sets time
			{
				TArray<const FAnimNotifyEvent*> Notifies;

				// now calculate time for each samples
				const float ClampedNormalizedPreviousTime = FMath::Clamp<float>(NormalizedPreviousTime, 0.f, 1.f);
				const float ClampedNormalizedCurrentTime = FMath::Clamp<float>(NormalizedCurrentTime, 0.f, 1.f);
				const bool bGenerateNotifies = Context.ShouldGenerateNotifies() && (NormalizedCurrentTime != NormalizedPreviousTime) && NotifyTriggerMode != ENotifyTriggerMode::None;
				int32 HighestWeightIndex = 0;

				// Get the index of the highest weight, assuming that the first is the highest until we find otherwise
				bool bTriggerNotifyHighestWeightedAnim = NotifyTriggerMode == ENotifyTriggerMode::HighestWeightedAnimation && SampleDataList.Num() > 0;
				if(bGenerateNotifies && bTriggerNotifyHighestWeightedAnim)
				{
					float HighestWeight = SampleDataList[HighestWeightIndex].GetWeight();
					for(int32 I = 1 ; I < SampleDataList.Num(); I++)
					{
						if(SampleDataList[I].GetWeight() > HighestWeight)
						{
							HighestWeightIndex = I;
							HighestWeight = SampleDataList[I].GetWeight();
						}
					}
				}
				
				for (int32 I = 0; I < SampleDataList.Num(); ++I)
				{
					FBlendSampleData& SampleEntry = SampleDataList[I];
					const int32 SampleDataIndex = SampleEntry.SampleDataIndex;

					// Skip SamplesPoints that has no relevant weight
					if( SampleData.IsValidIndex(SampleDataIndex) && (SampleEntry.TotalWeight > ZERO_ANIMWEIGHT_THRESH) )
					{
						const FBlendSample& Sample = SampleData[SampleDataIndex];
						if( Sample.Animation )
						{
							const float SampleNormalizedPreviousTime = Sample.Animation->RateScale >= 0.f ? ClampedNormalizedPreviousTime : 1.f - ClampedNormalizedPreviousTime;
							const float SampleNormalizedCurrentTime = Sample.Animation->RateScale >= 0.f ? ClampedNormalizedCurrentTime : 1.f - ClampedNormalizedCurrentTime;

							const float PrevSampleDataTime = SampleNormalizedPreviousTime * Sample.Animation->SequenceLength;
							float& CurrentSampleDataTime = SampleEntry.Time;
							CurrentSampleDataTime = SampleNormalizedCurrentTime * Sample.Animation->SequenceLength;

							// Figure out delta time 
							float DeltaTimePosition = CurrentSampleDataTime - PrevSampleDataTime;
							const float SampleMoveDelta = MoveDelta * Sample.Animation->RateScale;

							// if we went against play rate, then loop around.
							if ((SampleMoveDelta * DeltaTimePosition) < 0.f)
							{
								DeltaTimePosition += FMath::Sign<float>(SampleMoveDelta) * Sample.Animation->SequenceLength;
							}

							if( bGenerateNotifies && (!bTriggerNotifyHighestWeightedAnim || (I == HighestWeightIndex)))
							{
								// Harvest and record notifies
								Sample.Animation->GetAnimNotifies(PrevSampleDataTime, DeltaTimePosition, Instance.bLooping, Notifies);
							}

							if (Context.RootMotionMode == ERootMotionMode::RootMotionFromEverything && Sample.Animation->bEnableRootMotion)
							{
								Context.RootMotionMovementParams.AccumulateWithBlend(Sample.Animation->ExtractRootMotion(PrevSampleDataTime, DeltaTimePosition, Instance.bLooping), SampleEntry.GetWeight());
							}

							// handle curves
							Sample.Animation->EvaluateCurveData(InstanceOwner, CurrentSampleDataTime, Instance.EffectiveBlendWeight * SampleEntry.GetWeight());

							UE_LOG(LogAnimation, Verbose, TEXT("%d. Blending animation(%s) with %f weight at time %0.2f"), I+1, *Sample.Animation->GetName(), SampleEntry.GetWeight(), CurrentSampleDataTime);
						}
					}
				}

				if (bGenerateNotifies && Notifies.Num() > 0)
				{
					InstanceOwner->AddAnimNotifies(Notifies, Instance.EffectiveBlendWeight);
				}
			}
		}
	}
}

bool UBlendSpaceBase::GetSamplesFromBlendInput(const FVector &BlendInput, TArray<FBlendSampleData> & OutSampleDataList) const
{
	TArray<FGridBlendSample> RawGridSamples;
	GetRawSamplesFromBlendInput(BlendInput, RawGridSamples);

	OutSampleDataList.Empty();

	// consolidate all samples
	for (int32 SampleNum=0; SampleNum<RawGridSamples.Num(); ++SampleNum)
	{
		FGridBlendSample& GridSample = RawGridSamples[SampleNum];
		float GridWeight = GridSample.BlendWeight;
		FEditorElement& GridElement = GridSample.GridElement;

		for(int Ind = 0; Ind < GridElement.MAX_VERTICES; ++Ind)
		{
			if(GridElement.Indices[Ind] != INDEX_NONE)
			{
				int32 Index = OutSampleDataList.AddUnique(GridElement.Indices[Ind]);
				OutSampleDataList[Index].AddWeight(GridElement.Weights[Ind]*GridWeight);
			}
		}
	}

	/** Used to sort by  Weight. */
	struct FCompareFBlendSampleData
	{
		FORCEINLINE bool operator()( const FBlendSampleData& A, const FBlendSampleData& B ) const { return B.TotalWeight < A.TotalWeight; }
	};
	OutSampleDataList.Sort( FCompareFBlendSampleData() );

	// remove noisy ones
	int32 TotalSample = OutSampleDataList.Num();
	float TotalWeight = 0.f;
	for (int32 I=0; I<TotalSample; ++I)
	{
		if (OutSampleDataList[I].TotalWeight < ZERO_ANIMWEIGHT_THRESH)
		{
			// cut anything in front of this 
			OutSampleDataList.RemoveAt(I, TotalSample-I);
			break;
		}

		TotalWeight += OutSampleDataList[I].TotalWeight;
	}

	for (int32 I=0; I<OutSampleDataList.Num(); ++I)
	{
		// normalize to all weights
		OutSampleDataList[I].TotalWeight /= TotalWeight;
	}

	return (OutSampleDataList.Num()!=0);
}

// @todo fixme: slow approach. If the perbone gets popular, we should change this to array of weight 
// we should think about changing this to 
int32 UBlendSpaceBase::GetPerBoneInterpolationIndex(int32 BoneIndex, const FBoneContainer& RequiredBones) const
{
	for (int32 Iter=0; Iter<PerBoneBlend.Num(); ++Iter)
	{
		// we would like to make sure if 
		if (PerBoneBlend[Iter].BoneReference.IsValid(RequiredBones) && RequiredBones.BoneIsChildOf(BoneIndex, PerBoneBlend[Iter].BoneReference.BoneIndex))
		{
			return Iter;
		}
	}

	return INDEX_NONE;
}

bool UBlendSpaceBase::InterpolateWeightOfSampleData(float DeltaTime, const TArray<FBlendSampleData> & OldSampleDataList, const TArray<FBlendSampleData> & NewSampleDataList, TArray<FBlendSampleData> & FinalSampleDataList) const
{
	check (TargetWeightInterpolationSpeedPerSec > 0.f);

	float TotalFinalWeight = 0.f;

	// now interpolate from old to new target, this is brute-force
	for (auto OldIt=OldSampleDataList.CreateConstIterator(); OldIt; ++OldIt)
	{
		// Now need to modify old sample, so copy it
		FBlendSampleData OldSample = *OldIt;
		bool bNewTargetFound = false;

		if (OldSample.PerBoneBlendData.Num()!=PerBoneBlend.Num())
		{
			OldSample.PerBoneBlendData.Empty(PerBoneBlend.Num());
			OldSample.PerBoneBlendData.Init(OldSample.TotalWeight, PerBoneBlend.Num());
		}

		// i'd like to change this later
		for (auto NewIt=NewSampleDataList.CreateConstIterator(); NewIt; ++NewIt)
		{
			const FBlendSampleData& NewSample = *NewIt;
			// if same sample is found, interpolate
			if (NewSample.SampleDataIndex == OldSample.SampleDataIndex)
			{
				FBlendSampleData InterpData = NewSample;
				InterpData.TotalWeight = FMath::FInterpConstantTo(OldSample.TotalWeight, NewSample.TotalWeight, DeltaTime, TargetWeightInterpolationSpeedPerSec);
				InterpData.PerBoneBlendData = OldSample.PerBoneBlendData;

				// now interpolate the per bone weights
				for (int32 Iter = 0; Iter<InterpData.PerBoneBlendData.Num(); ++Iter)
				{
					InterpData.PerBoneBlendData[Iter] = FMath::FInterpConstantTo(OldSample.PerBoneBlendData[Iter], NewSample.TotalWeight, DeltaTime, PerBoneBlend[Iter].InterpolationSpeedPerSec);
				}

				FinalSampleDataList.Add(InterpData);
				TotalFinalWeight += InterpData.GetWeight();
				bNewTargetFound = true;
				break;
			}
		}

		// if new target isn't found, interpolate to 0.f, this is gone
		if (bNewTargetFound == false)
		{
			FBlendSampleData InterpData = OldSample;
			InterpData.TotalWeight = FMath::FInterpConstantTo(OldSample.TotalWeight, 0.f, DeltaTime, TargetWeightInterpolationSpeedPerSec);
			// now interpolate the per bone weights
			for (int32 Iter = 0; Iter<InterpData.PerBoneBlendData.Num(); ++Iter)
			{
				InterpData.PerBoneBlendData[Iter] = FMath::FInterpConstantTo(OldSample.PerBoneBlendData[Iter], 0.f, DeltaTime, PerBoneBlend[Iter].InterpolationSpeedPerSec);
			}

			// add it if it's not zero
			if ( InterpData.TotalWeight > ZERO_ANIMWEIGHT_THRESH )
			{
				FinalSampleDataList.Add(InterpData);
				TotalFinalWeight += InterpData.GetWeight();
			}
		}
	}

	// now find new samples that are not found from old samples
	for (auto OldIt=NewSampleDataList.CreateConstIterator(); OldIt; ++OldIt)
	{
		// Now need to modify old sample, so copy it
		FBlendSampleData OldSample = *OldIt;
		bool bNewTargetFound = false;

		if (OldSample.PerBoneBlendData.Num()!=PerBoneBlend.Num())
		{
			OldSample.PerBoneBlendData.Empty(PerBoneBlend.Num());
			OldSample.PerBoneBlendData.Init(OldSample.TotalWeight, PerBoneBlend.Num());
		}

		for (auto NewIt=FinalSampleDataList.CreateConstIterator(); NewIt; ++NewIt)
		{
			const FBlendSampleData& NewSample = *NewIt;
			if (NewSample.SampleDataIndex == OldSample.SampleDataIndex)
			{
				bNewTargetFound = true;
				break;
			}
		}

		// add those new samples
		if (bNewTargetFound == false)
		{
			FBlendSampleData InterpData = OldSample;
			InterpData.TotalWeight = FMath::FInterpConstantTo(0.f, OldSample.TotalWeight, DeltaTime, TargetWeightInterpolationSpeedPerSec);
			// now interpolate the per bone weights
			for (int32 Iter = 0; Iter<InterpData.PerBoneBlendData.Num(); ++Iter)
			{
				InterpData.PerBoneBlendData[Iter] = FMath::FInterpConstantTo(0.f, OldSample.PerBoneBlendData[Iter], DeltaTime, PerBoneBlend[Iter].InterpolationSpeedPerSec);
			}
			FinalSampleDataList.Add(InterpData);
			TotalFinalWeight += InterpData.GetWeight();
		}
	}

	return (TotalFinalWeight > ZERO_ANIMWEIGHT_THRESH);
}

bool UBlendSpaceBase::UpdateParameter(int32 Index, const FBlendParameter& Parameter)
{
	check ( Index >= 0 && Index <=2 );

	// too small
	if (Parameter.GetRange() < SMALL_NUMBER)
	{
		return false;
	}

	// mini grid number is 3
	if (Parameter.GridNum < 3)
	{
		return false;
	}

	// not valid, return
	if (Parameter.DisplayName.IsEmpty())
	{
		return false;
	}

	BlendParameters[Index] = Parameter;

	// now update all SamplePoint be within the range
	for (int32 I=0; I<SampleData.Num(); ++I)
	{
		SampleData[I].SampleValue = ClampBlendInput(SampleData[I].SampleValue);
	}

	ValidateSampleData();
	MarkPackageDirty();
	return true;
}

bool UBlendSpaceBase::AddSample(const FBlendSample& BlendSample)
{
	FBlendSample NewBlendSample = BlendSample;

	if ( ValidateSampleInput(NewBlendSample) == false )
	{
		return false;
	}

	SampleData.Add(NewBlendSample);
	GridSamples.Empty();

	MarkPackageDirty();
	return true;
}

bool UBlendSpaceBase::EditSample(const FBlendSample& BlendSample, FVector& NewValue)
{
	int32 NewIndex = SampleData.Find(BlendSample);
	if (NewIndex!=INDEX_NONE)
	{
		// edit is more work when we check if there is duplicate, so 
		// just delete old one, and add new one, 
		// when add it will verify if it isn't duplicate
		FBlendSample NewSample = BlendSample;
		NewSample.SampleValue = NewValue;

		if ( ValidateSampleInput(NewSample, NewIndex) == false )
		{
			return false;
		}

		SampleData[NewIndex] = NewSample;

#if WITH_EDITORONLY_DATA
		// fill upt PreviewBasePose if it does not set yet
		if ( IsValidAdditive() )
		{
			if ( PreviewBasePose == NULL && NewSample.Animation )
			{
				PreviewBasePose = NewSample.Animation->RefPoseSeq;
			}
		}
		// if not additive, clear pose
		else if ( PreviewBasePose )
		{
			PreviewBasePose = NULL;
		}
#endif // WITH_EDITORONLY_DATA

		// copy back the sample value, so it can display in the correct position
		NewValue = NewSample.SampleValue;
		MarkPackageDirty();
		return true;
	}

	return false;
}

bool UBlendSpaceBase::EditSampleAnimation(const FBlendSample& BlendSample, class UAnimSequence* AnimSequence)
{
	int32 NewIndex = SampleData.Find(BlendSample);
	if (NewIndex!=INDEX_NONE)
	{
		FBlendSample NewSample = BlendSample;
		NewSample.Animation = AnimSequence;

		if ( ValidateSampleInput(NewSample, NewIndex) == false )
		{
			return false;
		}

		SampleData[NewIndex] = NewSample;

		GridSamples.Empty();
		MarkPackageDirty();
		ValidateSampleData();
		return true;
	}

	return false;
}

bool UBlendSpaceBase::DeleteSample(const FBlendSample& BlendSample)
{
	int32 NewIndex = SampleData.Find(BlendSample);
	if (NewIndex!=INDEX_NONE)
	{
		SampleData.RemoveAt(NewIndex);
		GridSamples.Empty();
		MarkPackageDirty();
		ValidateSampleData();
		return true;
	}

	return false;
}

void UBlendSpaceBase::ClearAllSamples()
{
	SampleData.Empty();
	GridSamples.Empty();
	MarkPackageDirty();
	AnimLength = 0.f;
}

bool UBlendSpaceBase::ValidateSampleInput(FBlendSample & BlendSample, int32 OriginalIndex) const
{
	// make sure we get same kinds of samples(additive or nonadditive)
	if (SampleData.Num() > 0 && BlendSample.Animation)
	{
		if (IsValidAdditive() != (BlendSample.Animation->IsValidAdditive()))
		{
			UE_LOG(LogAnimation, Log, TEXT("Adding sample failed. Please add same kinds of sequence (additive/non-additive)."));
			return false;
		}
	}

	const USkeleton* MySkeleton = GetSkeleton();
	if (BlendSample.Animation &&
		(! MySkeleton ||
		! BlendSample.Animation->GetSkeleton()->IsCompatible(MySkeleton)))
	{
		UE_LOG(LogAnimation, Log, TEXT("Adding sample failed. Please add same kinds of sequence (additive/non-additive)."));
		return false;
	}

	SnapToBorder(BlendSample);
	// make sure blendsample is within the parameter range
	BlendSample.SampleValue = ClampBlendInput(BlendSample.SampleValue);

	if (IsTooCloseToExistingSamplePoint(BlendSample.SampleValue, OriginalIndex))
	{
		UE_LOG(LogAnimation, Log, TEXT("Adding sample failed. Too close to existing sample point."));
		return false;
	}

	return true;
}

void UBlendSpaceBase::ValidateSampleData()
{
	bool bMarkPackageDirty=false;
	AnimLength = 0.f;

	for (int32 I=0; I<SampleData.Num(); ++I)
	{
		if ( SampleData[I].Animation == 0 )
		{
			SampleData.RemoveAt(I);
			--I;

			bMarkPackageDirty = true;
			continue;
		}

		// we need data to be snapped on the border
		// otherwise, you have this grid area that doesn't have valid 
		// sample points. Usually users will put it there
		// if the value is around border, snap to border
		SnapToBorder(SampleData[I]);

		// see if same data exists, by same, same values
		for (int32 J=I+1; J<SampleData.Num(); ++J)
		{
			if ( IsSameSamplePoint(SampleData[I].SampleValue, SampleData[J].SampleValue) )
			{
				SampleData.RemoveAt(J);
				--J;

				bMarkPackageDirty = true;
			}
		}

		if (SampleData[I].Animation->SequenceLength > AnimLength)
		{
			// @todo : should apply scale? If so, we'll need to apply also when blend
			AnimLength = SampleData[I].Animation->SequenceLength;
		}
	}

	if (bMarkPackageDirty)
	{
		MarkPackageDirty();
	}
}

bool UBlendSpaceBase::IsTooCloseToExistingSamplePoint(const FVector& SampleValue, int32 OriginalIndex) const
{
	for (int32 I=0; I<SampleData.Num(); ++I)
	{
		if (I!=OriginalIndex)
		{
			if ( IsSameSamplePoint(SampleValue, SampleData[I].SampleValue) )
			{
				return true;
			}
		}
	}
	return false;
}

FVector UBlendSpaceBase::ClampBlendInput(const FVector& BlendInput)  const
{
	FVector ClampedBlendInput;

	ClampedBlendInput.X = FMath::Clamp<float>(BlendInput.X, BlendParameters[0].Min, BlendParameters[0].Max);
	ClampedBlendInput.Y = FMath::Clamp<float>(BlendInput.Y, BlendParameters[1].Min, BlendParameters[1].Max);
	ClampedBlendInput.Z = FMath::Clamp<float>(BlendInput.Z, BlendParameters[2].Min, BlendParameters[2].Max);
	return ClampedBlendInput;
}

FVector UBlendSpaceBase::GetNormalizedBlendInput(const FVector& BlendInput) const
{
	FVector MinBlendInput = FVector(BlendParameters[0].Min, BlendParameters[1].Min, BlendParameters[2].Min);
	FVector MaxBlendInput = FVector(BlendParameters[0].Max, BlendParameters[1].Max, BlendParameters[2].Max);

	FVector NormalizedBlendInput;

	NormalizedBlendInput.X = FMath::Clamp<float>(BlendInput.X, MinBlendInput.X, MaxBlendInput.X);
	NormalizedBlendInput.Y = FMath::Clamp<float>(BlendInput.Y, MinBlendInput.Y, MaxBlendInput.Y);
	NormalizedBlendInput.Z = FMath::Clamp<float>(BlendInput.Z, MinBlendInput.Z, MaxBlendInput.Z);

	NormalizedBlendInput -= MinBlendInput; 

	FVector GridSize = FVector(BlendParameters[0].GetGridSize(), BlendParameters[1].GetGridSize(), BlendParameters[2].GetGridSize());

	NormalizedBlendInput/=GridSize;

	return NormalizedBlendInput;
}

bool UBlendSpaceBase::IsValidAdditiveInternal(EAdditiveAnimationType AdditiveType) const
{
	TOptional<bool> bIsAdditive;

	for (int32 I=0; I<SampleData.Num(); ++I)
	{
		const UAnimSequence* Animation = SampleData[I].Animation;

		// test animation to see if it matched additive type
		bool bAdditiveAnim = ( Animation && Animation->IsValidAdditive() && Animation->AdditiveAnimType == AdditiveType );

		// if already has value, but it does not match
		if ( bIsAdditive.IsSet() )
		{
			// it's inconsistent, we ignore this
			if (bIsAdditive.GetValue() != bAdditiveAnim)
			{
				return false;
			}
		}
		else
		{
			bIsAdditive = TOptional<bool>(bAdditiveAnim);
		}
	}

	return (bIsAdditive.IsSet() && bIsAdditive.GetValue());
}

void UBlendSpaceBase::NormalizeSampleDataWeight(TArray<FBlendSampleData> & SampleDataList) const
{
	float TotalSum=0.f;

	TArray<float> PerBoneTotalSums;
	PerBoneTotalSums.AddZeroed(PerBoneBlend.Num());

	for (int32 I=0; I<SampleDataList.Num(); ++I)
	{
		const int32 SampleDataIndex = SampleDataList[I].SampleDataIndex;
		// need to verify if valid sample index
		if ( SampleData.IsValidIndex(SampleDataIndex) )
		{
			TotalSum += SampleDataList[I].GetWeight();

			if ( SampleDataList[I].PerBoneBlendData.Num() > 0 )
			{
				// now interpolate the per bone weights
				for (int32 Iter = 0; Iter<SampleDataList[I].PerBoneBlendData.Num(); ++Iter)
				{
					PerBoneTotalSums[Iter] += SampleDataList[I].PerBoneBlendData[Iter];
				}
			}
		}
	}

	if ( ensure (TotalSum > ZERO_ANIMWEIGHT_THRESH) )
	{
		for (int32 I=0; I<SampleDataList.Num(); ++I)
		{
			const int32 SampleDataIndex = SampleDataList[I].SampleDataIndex;
			if ( SampleData.IsValidIndex(SampleDataIndex) )
			{
				if (FMath::Abs<float>(TotalSum - 1.f) > ZERO_ANIMWEIGHT_THRESH)
				{
					SampleDataList[I].TotalWeight /= TotalSum;
				}

				// now interpolate the per bone weights
				for (int32 Iter = 0; Iter<SampleDataList[I].PerBoneBlendData.Num(); ++Iter)
				{
					if (FMath::Abs<float>(PerBoneTotalSums[Iter] - 1.f) > ZERO_ANIMWEIGHT_THRESH)
					{
						SampleDataList[I].PerBoneBlendData[Iter] /= PerBoneTotalSums[Iter];
					}
				}
			}
		}
	}
}

float UBlendSpaceBase::GetAnimationLengthFromSampleData(const TArray<FBlendSampleData> & SampleDataList) const
{
	float BlendAnimLength=0.f;
	for (int32 I=0; I<SampleDataList.Num(); ++I)
	{
		const int32 SampleDataIndex = SampleDataList[I].SampleDataIndex;
		if ( SampleData.IsValidIndex(SampleDataIndex) )
		{
			const FBlendSample& Sample = SampleData[SampleDataIndex];
			if (Sample.Animation)
			{
				// apply rate scale to get actual playback time
				BlendAnimLength += (Sample.Animation->SequenceLength / (Sample.Animation->RateScale != 0.0f ? FMath::Abs(Sample.Animation->RateScale) : 1.0f))*SampleDataList[I].GetWeight();
				UE_LOG(LogAnimation, Verbose, TEXT("[%d] - Sample Animation(%s) : Weight(%0.5f) "), I+1, *Sample.Animation->GetName(), SampleDataList[I].GetWeight());
			}
		}
	}

	return BlendAnimLength;
}

int32 UBlendSpaceBase::GetBlendSamplePoints(TArray<FBlendSample> & OutPointList) const
{
	OutPointList.Empty(SampleData.Num());
	OutPointList.AddUninitialized(SampleData.Num());

	for (int32 I=0; I<SampleData.Num(); ++I)
	{
		OutPointList[I] = SampleData[I];
	}

	return OutPointList.Num();
}

int32 UBlendSpaceBase::GetGridSamples(TArray<FEditorElement> & OutGridElements) const
{
	OutGridElements.Empty(GridSamples.Num());
	OutGridElements.AddUninitialized(GridSamples.Num());

	for (int32 I=0; I<GridSamples.Num(); ++I)
	{
		OutGridElements[I] = GridSamples[I];
	}

	return OutGridElements.Num();
}

void UBlendSpaceBase::FillupGridElements(const TArray<FVector> & PointList, const TArray<FEditorElement> & GridElements)
{
	// need to convert all GridElements indexing to PointList to the data I care
	// The data I care here is my SampleData

	// create new Map from PointList index to SampleData
	TArray<int32> PointListToSampleIndices;
	PointListToSampleIndices.Init(INDEX_NONE, PointList.Num());

	for (int32 I=0; I<PointList.Num(); ++I)
	{
		for (int32 J=0; J<SampleData.Num(); ++J)
		{
			if (SampleData[J].SampleValue == PointList[I])
			{
				PointListToSampleIndices[I] = J;
				break;
			}
		}
	}

	GridSamples.Empty(GridElements.Num());
	GridSamples.AddUninitialized(GridElements.Num());
	for (int32 I=0; I<GridElements.Num(); ++I)
	{
		const FEditorElement& ViewGrid = GridElements[I];
		FEditorElement NewGrid;
		float TotalWeight = 0.f;
		for (int32 J=0; J<FEditorElement::MAX_VERTICES; ++J)
		{
			if(ViewGrid.Indices[J] != INDEX_NONE)
			{
				NewGrid.Indices[J] = PointListToSampleIndices[ViewGrid.Indices[J]];
			}
			else
			{
				NewGrid.Indices[J] = INDEX_NONE;
			}

			if (NewGrid.Indices[J]==INDEX_NONE)
			{
				NewGrid.Weights[J] = 0.f;
			}
			else
			{
				NewGrid.Weights[J] = ViewGrid.Weights[J];
				TotalWeight += ViewGrid.Weights[J];
			}
		}

		// Need to renormalize
		if (TotalWeight>0.f)
		{
			for (int32 J=0; J<FEditorElement::MAX_VERTICES; ++J)
			{
				NewGrid.Weights[J]/=TotalWeight;
			}
		}

		GridSamples[I] = NewGrid;
	}
}

#if WITH_EDITOR
bool UBlendSpaceBase::GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences)
{
 	for (auto Iter = SampleData.CreateConstIterator(); Iter; ++Iter)
 	{
 		// saves all samples in the AnimSequences
 		AnimationSequences.AddUnique((*Iter).Animation);
 	}
 
 	return (AnimationSequences.Num() > 0);
}

void UBlendSpaceBase::ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap)
{
	TArray<FBlendSample> NewSamples;
	for (auto Iter = SampleData.CreateIterator(); Iter; ++Iter)
	{
		FBlendSample& Sample = (*Iter);
		UAnimSequence* Anim = Sample.Animation;

		if ( Anim )
		{
			UAnimSequence* const* ReplacementAsset = (UAnimSequence*const*)ReplacementMap.Find(Anim);
			if(ReplacementAsset)
			{
				Sample.Animation = *ReplacementAsset;
				NewSamples.Add(Sample);
			}
		}
	}
	SampleData = NewSamples;
}

void UBlendSpaceBase::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if ( PropertyName == FName(TEXT("InterpolationTime")) || PropertyName == FName(TEXT("InterpolationType")) )
	{
		bNeedReinitializeFilter = true;
	}

	if (PropertyName == FName(TEXT("BoneName")))
	{
		InitializePerBoneBlend();
	}
}
#endif // WITH_EDITOR