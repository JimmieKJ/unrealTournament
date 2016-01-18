// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AnimCurveTypes.h"
#include "Animation/AnimInstance.h"
#include "AnimationRuntime.h"

DECLARE_CYCLE_STAT(TEXT("BlendedCurve InitFrom"), STAT_BlendedCurve_InitFrom, STATGROUP_Anim);


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

float FFloatCurve::Evaluate(float CurrentTime) const
{
	return FloatCurve.Eval(CurrentTime);
}

void FFloatCurve::UpdateOrAddKey(float NewKey, float CurrentTime)
{
	FloatCurve.UpdateOrAddKey(CurrentTime, NewKey);
}

void FFloatCurve::Resize(float NewLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime)
{
	FloatCurve.ReadjustTimeRange(0, NewLength, bInsert, OldStartTime, OldEndTime);
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

void FVectorCurve::Resize(float NewLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime)
{
	FloatCurves[X].ReadjustTimeRange(0, NewLength, bInsert, OldStartTime, OldEndTime);
	FloatCurves[Y].ReadjustTimeRange(0, NewLength, bInsert, OldStartTime, OldEndTime);
	FloatCurves[Z].ReadjustTimeRange(0, NewLength, bInsert, OldStartTime, OldEndTime);
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

void FTransformCurve::Resize(float NewLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime)
{
	TranslationCurve.Resize(NewLength, bInsert, OldStartTime, OldEndTime);
	RotationCurve.Resize(NewLength, bInsert, OldStartTime, OldEndTime);
	ScaleCurve.Resize(NewLength, bInsert, OldStartTime, OldEndTime);
}

/////////////////////////////////////////////////////
// FRawCurveTracks

void FRawCurveTracks::EvaluateCurveData( FBlendedCurve& Curves, float CurrentTime ) const
{
	// evaluate the curve data at the CurrentTime and add to Instance
	for(auto CurveIter = FloatCurves.CreateConstIterator(); CurveIter; ++CurveIter)
	{
		const FFloatCurve& Curve = *CurveIter;
		Curves.Set(Curve.CurveUid, Curve.Evaluate(CurrentTime), Curve.GetCurveTypeFlags());
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

void FRawCurveTracks::DeleteAllCurveData(ESupportedCurveType SupportedCurveType /*= FloatType*/)
{
	switch(SupportedCurveType)
	{
#if WITH_EDITOR
	case VectorType:
		VectorCurves.Empty();
		break;
	case TransformType:
		TransformCurves.Empty();
		break;
#endif // WITH_EDITOR
	case FloatType:
	default:
		FloatCurves.Empty();
		break;
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

void FRawCurveTracks::Resize(float TotalLength, bool bInsert/* whether insert or remove*/, float OldStartTime, float OldEndTime)
{
	for (auto& Curve: FloatCurves)
	{
		Curve.Resize(TotalLength, bInsert, OldStartTime, OldEndTime);
	}

#if WITH_EDITORONLY_DATA
	for(auto& Curve: VectorCurves)
	{
		Curve.Resize(TotalLength, bInsert, OldStartTime, OldEndTime);
	}

	for(auto& Curve: TransformCurves)
	{
		Curve.Resize(TotalLength, bInsert, OldStartTime, OldEndTime);
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
	if (Ar.IsLoading())
	{
		SortFloatCurvesByUID();
	}
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
/////////////////////////////////////////////

FORCEINLINE void ValidateCurve(FBlendedCurve* Curve)
{
#if 0 
	for (auto& Element : Curve->Elements)
	{
		if (Element.Flags & ACF_DrivesMorphTarget)
		{
			ensure (Element.Value < 1.2f);
		}
	}
#endif
}

void FBlendedCurve::Set(USkeleton::AnimCurveUID InUid, float InValue, int32 InFlags)
{
	int32 ArrayIndex;

	check(bInitialized);

	if (UIDList.Find(InUid, ArrayIndex))
	{
		Elements[ArrayIndex].Value = InValue;
		Elements[ArrayIndex].Flags = InFlags;
	}

	ValidateCurve(this);
}

void FBlendedCurve::Reset(int32 Count)
{
	Elements.Reset();
	Elements.Reserve(Count);
}

//@Todo curve flags won't transfer over - it only overwrites
void FBlendedCurve::Blend(const FBlendedCurve& A, const FBlendedCurve& B, float Alpha)
{
	check(A.Num()==B.Num());
	if(FMath::Abs(Alpha) <= ZERO_ANIMWEIGHT_THRESH)
	{
		// if blend is all the way for child1, then just copy its bone atoms
		Override(A);
	}
	else if(FMath::Abs(Alpha - 1.0f) <= ZERO_ANIMWEIGHT_THRESH)
	{
		// if blend is all the way for child2, then just copy its bone atoms
		Override(B);
	}
	else
	{
		InitFrom(A);
		for(int32 CurveId=0; CurveId<A.Elements.Num(); ++CurveId)
		{
			Elements[CurveId].Value = FMath::Lerp(A.Elements[CurveId].Value, B.Elements[CurveId].Value, Alpha); 
			Elements[CurveId].Flags = (A.Elements[CurveId].Flags) | (B.Elements[CurveId].Flags);
		}
	}

	ValidateCurve(this);
}

void FBlendedCurve::BlendWith(const FBlendedCurve& Other, float Alpha)
{
	check(Num()==Other.Num());
	if(FMath::Abs(Alpha) <= ZERO_ANIMWEIGHT_THRESH)
	{
		return;
	}
	else if(FMath::Abs(Alpha - 1.0f) <= ZERO_ANIMWEIGHT_THRESH)
	{
		// if blend is all the way for child2, then just copy its bone atoms
		Override(Other);
	}
	else
	{
		for(int32 CurveId=0; CurveId<Elements.Num(); ++CurveId)
		{
			Elements[CurveId].Value = FMath::Lerp(Elements[CurveId].Value, Other.Elements[CurveId].Value, Alpha);
			Elements[CurveId].Flags |= (Other.Elements[CurveId].Flags);
		}
	}
	 
	ValidateCurve(this);
}

void FBlendedCurve::ConvertToAdditive(const FBlendedCurve& BaseCurve)
{
	check(bInitialized);
	check(Num()==BaseCurve.Num());

	for(int32 CurveId=0; CurveId<Elements.Num(); ++CurveId)
	{
		Elements[CurveId].Value -= BaseCurve.Elements[CurveId].Value;
		Elements[CurveId].Flags |= BaseCurve.Elements[CurveId].Flags;
	}
	ValidateCurve(this);
}

void FBlendedCurve::Accumulate(const FBlendedCurve& AdditiveCurve, float Weight)
{
	check(bInitialized);
	check(Num()==AdditiveCurve.Num());

	if (Weight > ZERO_ANIMWEIGHT_THRESH)
	{
		for(int32 CurveId=0; CurveId<Elements.Num(); ++CurveId)
		{
			Elements[CurveId].Value += AdditiveCurve.Elements[CurveId].Value * Weight;
			Elements[CurveId].Flags |= AdditiveCurve.Elements[CurveId].Flags;
		}
	}

	ValidateCurve(this);
}

void FBlendedCurve::Override(const FBlendedCurve& CurveToOverrideFrom, float Weight)
{
	InitFrom(CurveToOverrideFrom);

	if ( FMath::IsNearlyEqual(Weight, 1.f) )
	{
		Override(CurveToOverrideFrom);
	}
	else
	{
		for(int32 CurveId=0; CurveId<CurveToOverrideFrom.Elements.Num(); ++CurveId)
		{
			Elements[CurveId].Value = CurveToOverrideFrom.Elements[CurveId].Value * Weight;
			Elements[CurveId].Flags |= CurveToOverrideFrom.Elements[CurveId].Flags;
		}
	}

	ValidateCurve(this);
}

void FBlendedCurve::Override(const FBlendedCurve& CurveToOverrideFrom)
{
	InitFrom(CurveToOverrideFrom);
	Elements.Reset();
	Elements.Append(CurveToOverrideFrom.Elements);

	ValidateCurve(this);
}

void FBlendedCurve::InitFrom(const USkeleton* Skeleton)
{
	SCOPE_CYCLE_COUNTER(STAT_BlendedCurve_InitFrom);

	if(const FSmartNameMapping* Mapping = Skeleton->SmartNames.GetContainer(USkeleton::AnimCurveMappingName))
	{
		Mapping->FillUidArray(UIDList);
		Elements.Reset();
		Elements.AddZeroed(UIDList.Num());
	}

	// no name, means no curve
	bInitialized = true;
}

void FBlendedCurve::InitFrom(const FBlendedCurve& InCurveToInitFrom)
{
	SCOPE_CYCLE_COUNTER(STAT_BlendedCurve_InitFrom);

	// make sure this doesn't happen
	if (ensure(&InCurveToInitFrom != this))
	{
		UIDList.Reset();
		UIDList.Append(InCurveToInitFrom.UIDList);
		Elements.Reset();
		Elements.AddZeroed(UIDList.Num());
		bInitialized = true;
	}
}

FBlendedCurve::FBlendedCurve(const class UAnimInstance* AnimInstance)
{
	check(AnimInstance);
	InitFrom(AnimInstance->CurrentSkeleton);
}

void FBlendedCurve::CopyFrom(const FBlendedCurve& CurveToCopyFrom)
{
	if (&CurveToCopyFrom != this)
	{
		UIDList.Reset();
		UIDList.Append(CurveToCopyFrom.UIDList);
		Elements.Reset();
		Elements.Append(CurveToCopyFrom.Elements);
		bInitialized = true;
		ValidateCurve(this);
	}
}

void FBlendedCurve::MoveFrom(FBlendedCurve& CurveToMoveFrom)
{
	UIDList = MoveTemp(CurveToMoveFrom.UIDList);
	Elements = MoveTemp(CurveToMoveFrom.Elements);
	bInitialized = true;
	CurveToMoveFrom.bInitialized = false;

	ValidateCurve(this);
}

void FBlendedCurve::Combine(const FBlendedCurve& CurveToCombine)
{
	check(bInitialized);
	check(Num()==CurveToCombine.Num());

	for(int32 CurveId=0; CurveId<CurveToCombine.Elements.Num(); ++CurveId)
	{
		// if target value is non zero, we accpet target's value
		// originally this code was doing max, but that doesn't make sense since the values can be negative
		// we could try to pick non-zero, but if target value is non-zero, I think we should accept that value 
		// if source is non zero, it will be overriden
		if (CurveToCombine.Elements[CurveId].Value != 0.f)
		{
			Elements[CurveId].Value = CurveToCombine.Elements[CurveId].Value; 
		}

		Elements[CurveId].Flags |= CurveToCombine.Elements[CurveId].Flags;
	}

	ValidateCurve(this);
}