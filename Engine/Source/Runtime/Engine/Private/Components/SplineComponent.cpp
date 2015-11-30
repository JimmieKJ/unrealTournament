// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Spline.cpp
=============================================================================*/

#include "EnginePrivate.h"
#include "Components/SplineComponent.h"
#include "ComponentInstanceDataCache.h"


USplineComponent::USplineComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAllowSplineEditingPerInstance_DEPRECATED(true)
	, ReparamStepsPerSegment(10)
	, Duration(1.0f)
	, bStationaryEndpoints(false)
	, bSplineHasBeenEdited(false)
	, bClosedLoop(false)
	, DefaultUpVector(FVector::UpVector)
#if WITH_EDITORONLY_DATA
	, EditorUnselectedSplineSegmentColor(FLinearColor(1.0f, 1.0f, 1.0f))
	, EditorSelectedSplineSegmentColor(FLinearColor(1.0f, 0.0f, 0.0f))
	, bShouldVisualizeScale(false)
	, ScaleVisualizationWidth(30.0f)
#endif
{
	SplineInfo.Points.Reset(10);
	SplineRotInfo.Points.Reset(10);
	SplineScaleInfo.Points.Reset(10);

	SplineInfo.Points.Emplace(0.0f, FVector(0, 0, 0), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
	SplineRotInfo.Points.Emplace(0.0f, FQuat::Identity, FQuat::Identity, FQuat::Identity, CIM_CurveAuto);
	SplineScaleInfo.Points.Emplace(0.0f, FVector(1.0f), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);

	SplineInfo.Points.Emplace(1.0f, FVector(100, 0, 0), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
	SplineRotInfo.Points.Emplace(1.0f, FQuat::Identity, FQuat::Identity, FQuat::Identity, CIM_CurveAuto);
	SplineScaleInfo.Points.Emplace(1.0f, FVector(1.0f), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);

	UpdateSpline();
}


static EInterpCurveMode ConvertSplinePointTypeToInterpCurveMode(ESplinePointType::Type SplinePointType)
{
	switch (SplinePointType)
	{
		case ESplinePointType::Linear:				return CIM_Linear;
		case ESplinePointType::Curve:				return CIM_CurveAuto;
		case ESplinePointType::Constant:			return CIM_Constant;
		case ESplinePointType::CurveCustomTangent:	return CIM_CurveUser;
		case ESplinePointType::CurveClamped:		return CIM_CurveAutoClamped;

		default:									return CIM_Unknown;
	}
}

static ESplinePointType::Type ConvertInterpCurveModeToSplinePointType(EInterpCurveMode InterpCurveMode)
{
	switch (InterpCurveMode)
	{
		case CIM_Linear:			return ESplinePointType::Linear;
		case CIM_CurveAuto:			return ESplinePointType::Curve;
		case CIM_Constant:			return ESplinePointType::Constant;
		case CIM_CurveUser:			return ESplinePointType::CurveCustomTangent;
		case CIM_CurveAutoClamped:	return ESplinePointType::CurveClamped;

		default:					return ESplinePointType::Constant;
	}
}


#if WITH_EDITOR
void USplineComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Support old resources which don't have the rotation and scale splines present
	const int32 ArchiveUE4Version = Ar.UE4Ver();
	if (ArchiveUE4Version < VER_UE4_INTERPCURVE_SUPPORTS_LOOPING)
	{
		int32 NumPoints = SplineInfo.Points.Num();

		// The start point is no longer cloned as the endpoint when the spline is looped, so remove the extra endpoint if present
		const bool bHasExtraEndpoint = bClosedLoop && (SplineInfo.Points[0].OutVal == SplineInfo.Points[NumPoints - 1].OutVal);

		if (bHasExtraEndpoint)
		{
			SplineInfo.Points.RemoveAt(NumPoints - 1, 1, false);
			NumPoints--;
		}

		// Fill the other two splines with some defaults
		SplineRotInfo.Points.Reset(NumPoints);
		SplineScaleInfo.Points.Reset(NumPoints);
		for (int32 Count = 0; Count < NumPoints; Count++)
		{
			SplineRotInfo.Points.Emplace(0.0f, FQuat::Identity, FQuat::Identity, FQuat::Identity, CIM_CurveAuto);
			SplineScaleInfo.Points.Emplace(0.0f, FVector(1.0f), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
		}

		USplineComponent* Archetype = CastChecked<USplineComponent>(GetArchetype());
		bSplineHasBeenEdited = (SplineInfo.Points != Archetype->SplineInfo.Points);
		UpdateSpline();
	}
}
#endif


void USplineComponent::PostLoad()
{
	Super::PostLoad();
}


void USplineComponent::PostEditImport()
{
	Super::PostEditImport();
}


void USplineComponent::UpdateSpline()
{
	const int32 NumPoints = SplineInfo.Points.Num();
	check(SplineRotInfo.Points.Num() == NumPoints && SplineScaleInfo.Points.Num() == NumPoints);

	// Ensure input keys are ascending in steps of 1.0
	for (int32 Index = 0; Index < NumPoints; Index++)
	{
		const float InVal = static_cast<float>(Index);
		SplineInfo.Points[Index].InVal = InVal;
		SplineRotInfo.Points[Index].InVal = InVal;
		SplineScaleInfo.Points[Index].InVal = InVal;
	}

	// Nothing else to do if less than 2 points
	if (NumPoints < 2)
	{
		return;
	}

	// Ensure splines' looping status matches with that of the spline component
	if (bClosedLoop)
	{
		const float LoopKey = static_cast<float>(NumPoints);
		SplineInfo.SetLoopKey(LoopKey);
		SplineRotInfo.SetLoopKey(LoopKey);
		SplineScaleInfo.SetLoopKey(LoopKey);
	}
	else
	{
		SplineInfo.ClearLoopKey();
		SplineRotInfo.ClearLoopKey();
		SplineScaleInfo.ClearLoopKey();
	}

	// Automatically set the tangents on any CurveAuto keys
	SplineInfo.AutoSetTangents(0.0f, bStationaryEndpoints);
	SplineRotInfo.AutoSetTangents(0.0f, bStationaryEndpoints);
	SplineScaleInfo.AutoSetTangents(0.0f, bStationaryEndpoints);

	// Now initialize the spline reparam table
	const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;

	// Start by clearing it
	SplineReparamTable.Points.Reset(NumSegments * ReparamStepsPerSegment + 1);
	float AccumulatedLength = 0.0f;
	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		for (int32 Step = 0; Step < ReparamStepsPerSegment; ++Step)
		{
			const float Param = static_cast<float>(Step) / ReparamStepsPerSegment;
			const float SegmentLength = (Step == 0) ? 0.0f : GetSegmentLength(SegmentIndex, Param);

			SplineReparamTable.Points.Emplace(SegmentLength + AccumulatedLength, SegmentIndex + Param, 0.0f, 0.0f, CIM_Linear);
		}
		AccumulatedLength += GetSegmentLength(SegmentIndex, 1.0f);
	}

	SplineReparamTable.Points.Emplace(AccumulatedLength, static_cast<float>(NumSegments), 0.0f, 0.0f, CIM_Linear);
}


float USplineComponent::GetSegmentLength(const int32 Index, const float Param) const
{
	const int32 NumPoints = SplineInfo.Points.Num();
	const int32 LastPoint = NumPoints - 1;

	check(Index >= 0 && ((bClosedLoop && Index < NumPoints) || (!bClosedLoop && Index < LastPoint)));
	check(Param >= 0.0f && Param <= 1.0f);

	// Evaluate the length of a Hermite spline segment.
	// This calculates the integral of |dP/dt| dt, where P(t) is the spline equation with components (x(t), y(t), z(t)).
	// This isn't solvable analytically, so we use a numerical method (Legendre-Gauss quadrature) which performs very well
	// with functions of this type, even with very few samples.  In this case, just 5 samples is sufficient to yield a
	// reasonable result.

	struct FLegendreGaussCoefficient
	{
		float Abscissa;
		float Weight;
	};

	static const FLegendreGaussCoefficient LegendreGaussCoefficients[] =
	{
		{ 0.0f, 0.5688889f },
		{ -0.5384693f, 0.47862867f },
		{ 0.5384693f, 0.47862867f },
		{ -0.90617985f, 0.23692688f },
		{ 0.90617985f, 0.23692688f }
	};

	const auto& StartPoint = SplineInfo.Points[Index];
	const auto& EndPoint = SplineInfo.Points[Index == LastPoint ? 0 : Index + 1];
	check(Index == LastPoint || (static_cast<int32>(EndPoint.InVal) - static_cast<int32>(StartPoint.InVal) == 1));

	const auto& P0 = StartPoint.OutVal;
	const auto& T0 = StartPoint.LeaveTangent;
	const auto& P1 = EndPoint.OutVal;
	const auto& T1 = EndPoint.ArriveTangent;

	// Cache the coefficients to be fed into the function to calculate the spline derivative at each sample point as they are constant.
	const FVector Coeff1 = ((P0 - P1) * 2.0f + T0 + T1) * 3.0f;
	const FVector Coeff2 = (P1 - P0) * 6.0f - T0 * 4.0f - T1 * 2.0f;
	const FVector Coeff3 = T0;

	const float HalfParam = Param * 0.5f;

	float Length = 0.0f;
	for (const auto& LegendreGaussCoefficient : LegendreGaussCoefficients)
	{
		// Calculate derivative at each Legendre-Gauss sample, and perform a weighted sum
		const float Alpha = HalfParam * (1.0f + LegendreGaussCoefficient.Abscissa);
		const FVector Derivative = ((Coeff1 * Alpha + Coeff2) * Alpha + Coeff3) * ComponentToWorld.GetScale3D();
		Length += Derivative.Size() * LegendreGaussCoefficient.Weight;
	}
	Length *= HalfParam;

	return Length;
}


float USplineComponent::GetSegmentParamFromLength(const int32 Index, const float Length, const float SegmentLength) const
{
	if (SegmentLength == 0.0f)
	{
		return 0.0f;
	}

	// Given a function P(x) which yields points along a spline with x = 0...1, we can define a function L(t) to be the
	// Euclidian length of the spline from P(0) to P(t):
	//
	//    L(t) = integral of |dP/dt| dt
	//         = integral of sqrt((dx/dt)^2 + (dy/dt)^2 + (dz/dt)^2) dt
	//
	// This method evaluates the inverse of this function, i.e. given a length d, it obtains a suitable value for t such that:
	//    L(t) - d = 0
	//
	// We use Newton-Raphson to iteratively converge on the result:
	//
	//    t' = t - f(t) / (df/dt)
	//
	// where: t is an initial estimate of the result, obtained through basic linear interpolation,
	//        f(t) is the function whose root we wish to find = L(t) - d,
	//        (df/dt) = d(L(t))/dt = |dP/dt|

	const int32 NumPoints = SplineInfo.Points.Num();
	const int32 LastPoint = NumPoints - 1;

	check(Index >= 0 && ((bClosedLoop && Index < NumPoints) || (!bClosedLoop && Index < LastPoint)));
	check(Length >= 0.0f && Length <= SegmentLength);
	check(Index == LastPoint || (static_cast<int32>(SplineInfo.Points[Index + 1].InVal) - static_cast<int32>(SplineInfo.Points[Index].InVal) == 1));

	float Param = Length / SegmentLength;  // initial estimate for t

	// two iterations of Newton-Raphson is enough
	for (int32 Iteration = 0; Iteration < 2; ++Iteration)
	{
		float TangentMagnitude = SplineInfo.EvalDerivative(Index + Param, FVector::ZeroVector).Size();
		if (TangentMagnitude > 0.0f)
		{
			Param -= (GetSegmentLength(Index, Param) - Length) / TangentMagnitude;
			Param = FMath::Clamp(Param, 0.0f, 1.0f);
		}
	}

	return Param;
}


FVector USplineComponent::GetLocationAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FVector Location = SplineInfo.Eval(InKey, FVector::ZeroVector);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Location = ComponentToWorld.TransformPosition(Location);
	}

	return Location;
}


FVector USplineComponent::GetTangentAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FVector Tangent = SplineInfo.EvalDerivative(InKey, FVector::ZeroVector);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Tangent = ComponentToWorld.TransformVector(Tangent);
	}

	return Tangent;
}


FVector USplineComponent::GetDirectionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FVector Direction = SplineInfo.EvalDerivative(InKey, FVector::ZeroVector).GetSafeNormal();

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Direction = ComponentToWorld.TransformVectorNoScale(Direction);
	}

	return Direction;
}


FRotator USplineComponent::GetRotationAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetQuaternionAtSplineInputKey(InKey, CoordinateSpace).Rotator();
}


FQuat USplineComponent::GetQuaternionAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	FQuat Quat = SplineRotInfo.Eval(InKey, FQuat::Identity);
	Quat.Normalize();

	const FVector Direction = SplineInfo.EvalDerivative(InKey, FVector::ZeroVector).GetSafeNormal();
	const FVector UpVector = Quat.RotateVector(DefaultUpVector);

	FQuat Rot = (FRotationMatrix::MakeFromXZ(Direction, UpVector)).ToQuat();

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Rot = ComponentToWorld.GetRotation() * Rot;
	}

	return Rot;
}


FVector USplineComponent::GetUpVectorAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const FQuat Quat = GetQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local);
	FVector UpVector = Quat.RotateVector(FVector::UpVector);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		UpVector = ComponentToWorld.TransformVectorNoScale(UpVector);
	}

	return UpVector;
}


FVector USplineComponent::GetRightVectorAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const FQuat Quat = GetQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local);
	FVector RightVector = Quat.RotateVector(FVector::RightVector);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		RightVector = ComponentToWorld.TransformVectorNoScale(RightVector);
	}

	return RightVector;
}


FTransform USplineComponent::GetTransformAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale) const
{
	const FVector Location(GetLocationAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FQuat Rotation(GetQuaternionAtSplineInputKey(InKey, ESplineCoordinateSpace::Local));
	const FVector Scale = bUseScale ? GetScaleAtSplineInputKey(InKey) : FVector(1.0f);

	FTransform Transform(Rotation, Location, Scale);

	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		Transform = Transform * ComponentToWorld;
	}

	return Transform;
}


float USplineComponent::GetRollAtSplineInputKey(float InKey, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetRotationAtSplineInputKey(InKey, CoordinateSpace).Roll;
}


FVector USplineComponent::GetScaleAtSplineInputKey(float InKey) const
{
	const FVector Scale = SplineScaleInfo.Eval(InKey, FVector(1.0f));
	return Scale;
}


void USplineComponent::SetClosedLoop(bool bInClosedLoop)
{
	bClosedLoop = bInClosedLoop;
	UpdateSpline();
}


bool USplineComponent::IsClosedLoop() const
{
	return bClosedLoop;
}


void USplineComponent::SetUnselectedSplineSegmentColor(const FLinearColor& Color)
{
#if WITH_EDITOR
	EditorUnselectedSplineSegmentColor = Color;
#endif
}


void USplineComponent::SetSelectedSplineSegmentColor(const FLinearColor& Color)
{
#if WITH_EDITOR
	EditorSelectedSplineSegmentColor = Color;
#endif
}


void USplineComponent::ClearSplinePoints()
{
	SplineInfo.Points.Reset();
	SplineRotInfo.Points.Reset();
	SplineScaleInfo.Points.Reset();
	UpdateSpline();
}


void USplineComponent::AddSplinePoint(const FVector& Position, ESplineCoordinateSpace::Type CoordinateSpace)
{
	const FVector TransformedPosition = (CoordinateSpace == ESplineCoordinateSpace::World) ?
		ComponentToWorld.InverseTransformPosition(Position) : Position;

	const float InKey = static_cast<float>(SplineInfo.Points.Num());

	SplineInfo.Points.Emplace(InKey, TransformedPosition, FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
	SplineRotInfo.Points.Emplace(InKey, FQuat::Identity, FQuat::Identity, FQuat::Identity, CIM_CurveAuto);
	SplineScaleInfo.Points.Emplace(InKey, FVector(1.0f), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);

	UpdateSpline();
}

void USplineComponent::AddSplinePointAtIndex(const FVector& Position, int32 Index, ESplineCoordinateSpace::Type CoordinateSpace)
{
	const FVector TransformedPosition = (CoordinateSpace == ESplineCoordinateSpace::World) ?
		ComponentToWorld.InverseTransformPosition(Position) : Position;

	const float InKey = static_cast<float>(Index);

	if (((Index >= 0) && 
		(Index < SplineInfo.Points.Num())) &&
		(Index < SplineRotInfo.Points.Num()) &&
		(Index < SplineScaleInfo.Points.Num()))
	{
		SplineInfo.Points.Insert(FInterpCurvePoint<FVector>(InKey, TransformedPosition, FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto), Index);
		SplineRotInfo.Points.Insert(FInterpCurvePoint<FQuat>(InKey, FQuat::Identity, FQuat::Identity, FQuat::Identity, CIM_CurveAuto), Index);
		SplineScaleInfo.Points.Insert(FInterpCurvePoint<FVector>(InKey, FVector(1.0f), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto), Index);
	}

	UpdateSpline();
}

void USplineComponent::RemoveSplinePoint(const int32 Index)
{
	int32 Count = 1;

	if ((Index >= 0) &&
		(SplineInfo.Points.Num() >= 0) &&
		(SplineRotInfo.Points.Num() >= 0) &&
		(SplineScaleInfo.Points.Num() >= 0) &&
		(Index + Count <= SplineInfo.Points.Num()) &&
		(Index + Count <= SplineRotInfo.Points.Num()) &&
		(Index + Count <= SplineScaleInfo.Points.Num())
		)
	{
		SplineInfo.Points.RemoveAt(Index, Count, false);
		SplineRotInfo.Points.RemoveAt(Index, Count, false);
		SplineScaleInfo.Points.RemoveAt(Index, Count, false);
	}

	UpdateSpline();
}

void USplineComponent::SetSplinePoints(const TArray<FVector>& Points, ESplineCoordinateSpace::Type CoordinateSpace)
{
	const int32 NumPoints = Points.Num();
	SplineInfo.Points.Reset(NumPoints);
	SplineRotInfo.Points.Reset(NumPoints);
	SplineScaleInfo.Points.Reset(NumPoints);

	float InputKey = 0.0f;
	for (const auto& Point : Points)
	{
		const FVector TransformedPoint = (CoordinateSpace == ESplineCoordinateSpace::World) ?
			ComponentToWorld.InverseTransformPosition(Point) : Point;

		SplineInfo.Points.Emplace(InputKey, TransformedPoint, FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);
		SplineRotInfo.Points.Emplace(InputKey, FQuat::Identity, FQuat::Identity, FQuat::Identity, CIM_CurveAuto);
		SplineScaleInfo.Points.Emplace(InputKey, FVector(1.0f), FVector::ZeroVector, FVector::ZeroVector, CIM_CurveAuto);

		InputKey += 1.0f;
	}

	UpdateSpline();
}


void USplineComponent::SetLocationAtSplinePoint(int32 PointIndex, const FVector& InLocation, ESplineCoordinateSpace::Type CoordinateSpace)
{
	const int32 NumPoints = SplineInfo.Points.Num();

	if ((PointIndex >= 0) && (PointIndex < NumPoints))
	{
		const FVector TransformedLocation = (CoordinateSpace == ESplineCoordinateSpace::World) ?
			ComponentToWorld.InverseTransformPosition(InLocation) : InLocation;

		SplineInfo.Points[PointIndex].OutVal = TransformedLocation;

		UpdateSpline();
	}
}


void USplineComponent::SetTangentAtSplinePoint(int32 PointIndex, const FVector& InTangent, ESplineCoordinateSpace::Type CoordinateSpace)
{
	const int32 NumPoints = SplineInfo.Points.Num();

	if ((PointIndex >= 0) && (PointIndex < NumPoints))
	{
		const FVector TransformedTangent = (CoordinateSpace == ESplineCoordinateSpace::World) ?
			ComponentToWorld.InverseTransformVector(InTangent) : InTangent;

		SplineInfo.Points[PointIndex].LeaveTangent = TransformedTangent;
		SplineInfo.Points[PointIndex].ArriveTangent = TransformedTangent;
		SplineInfo.Points[PointIndex].InterpMode = CIM_CurveUser;

		UpdateSpline();
	}
}


ESplinePointType::Type USplineComponent::GetSplinePointType(int32 PointIndex) const
{
	if ((PointIndex >= 0) && (PointIndex < SplineInfo.Points.Num()))
	{
		return ConvertInterpCurveModeToSplinePointType(SplineInfo.Points[PointIndex].InterpMode);
	}

	return ESplinePointType::Constant;
}


void USplineComponent::SetSplinePointType(int32 PointIndex, ESplinePointType::Type Type)
{
	if ((PointIndex >= 0) && (PointIndex < SplineInfo.Points.Num()))
	{
		SplineInfo.Points[PointIndex].InterpMode = ConvertSplinePointTypeToInterpCurveMode(Type);
		UpdateSpline();
	}
}


int32 USplineComponent::GetNumberOfSplinePoints() const
{
	// No longer returns an imaginary extra endpoint if closed loop is set
	const int32 NumPoints = SplineInfo.Points.Num();
	return NumPoints;
}


FVector USplineComponent::GetLocationAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetLocationAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace);
}


FVector USplineComponent::GetDirectionAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetLocationAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace);
}


FVector USplineComponent::GetTangentAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetTangentAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace);
}


FQuat USplineComponent::GetQuaternionAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetQuaternionAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace);
}


FRotator USplineComponent::GetRotationAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetRotationAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace);
}


FVector USplineComponent::GetUpVectorAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetUpVectorAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace);
}


FVector USplineComponent::GetRightVectorAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetRightVectorAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace);
}


float USplineComponent::GetRollAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	return GetRollAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace);
}


FVector USplineComponent::GetScaleAtSplinePoint(int32 PointIndex) const
{
	return GetScaleAtSplineInputKey(static_cast<float>(PointIndex));
}


FTransform USplineComponent::GetTransformAtSplinePoint(int32 PointIndex, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale) const
{
	return GetTransformAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace, bUseScale);
}


void USplineComponent::GetLocationAndTangentAtSplinePoint(int32 PointIndex, FVector& Location, FVector& Tangent, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	Location = GetLocationAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace);
	Tangent = GetTangentAtSplineInputKey(static_cast<float>(PointIndex), CoordinateSpace);
}


float USplineComponent::GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const
{
	const int32 NumPoints = SplineInfo.Points.Num();
	const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;

	if ((PointIndex >= 0) && (PointIndex < NumSegments + 1))
	{
		return SplineReparamTable.Points[PointIndex * ReparamStepsPerSegment].InVal;
	}

	return 0.0f;
}


float USplineComponent::GetSplineLength() const
{
	const int32 NumPoints = SplineReparamTable.Points.Num();

	// This is given by the input of the last entry in the remap table
	if (NumPoints > 0)
	{
		return SplineReparamTable.Points.Last().InVal;
	}
	
	return 0.0f;
}


void USplineComponent::SetDefaultUpVector(const FVector& UpVector, ESplineCoordinateSpace::Type CoordinateSpace)
{
	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		DefaultUpVector = ComponentToWorld.InverseTransformVector(UpVector);
	}
	else
	{
		DefaultUpVector = UpVector;
	}

	UpdateSpline();
}


FVector USplineComponent::GetDefaultUpVector(ESplineCoordinateSpace::Type CoordinateSpace) const
{
	if (CoordinateSpace == ESplineCoordinateSpace::World)
	{
		return ComponentToWorld.TransformVector(DefaultUpVector);
	}
	else
	{
		return DefaultUpVector;
	}
}


float USplineComponent::GetInputKeyAtDistanceAlongSpline(float Distance) const
{
	const int32 NumPoints = SplineInfo.Points.Num();

	if (NumPoints < 2)
	{
		return 0.0f;
	}

	const float TimeMultiplier = Duration / (NumPoints - 1.0f);
	return SplineReparamTable.Eval(Distance, 0.0f) * TimeMultiplier;
}


FVector USplineComponent::GetLocationAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.0f);
	return GetLocationAtSplineInputKey(Param, CoordinateSpace);
}


FVector USplineComponent::GetTangentAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.0f);
	return GetTangentAtSplineInputKey(Param, CoordinateSpace);
}


FVector USplineComponent::GetDirectionAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.0f);
	return GetDirectionAtSplineInputKey(Param, CoordinateSpace);
}


FQuat USplineComponent::GetQuaternionAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.0f);
	return GetQuaternionAtSplineInputKey(Param, CoordinateSpace);
}


FRotator USplineComponent::GetRotationAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.0f);
	return GetRotationAtSplineInputKey(Param, CoordinateSpace);
}


FVector USplineComponent::GetUpVectorAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.0f);
	return GetUpVectorAtSplineInputKey(Param, CoordinateSpace);
}


FVector USplineComponent::GetRightVectorAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.0f);
	return GetRightVectorAtSplineInputKey(Param, CoordinateSpace);
}


float USplineComponent::GetRollAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.0f);
	return GetRollAtSplineInputKey(Param, CoordinateSpace);
}


FVector USplineComponent::GetScaleAtDistanceAlongSpline(float Distance) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.0f);
	return GetScaleAtSplineInputKey(Param);
}


FTransform USplineComponent::GetTransformAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.0f);
	return GetTransformAtSplineInputKey(Param, CoordinateSpace, bUseScale);
}


FVector USplineComponent::GetLocationAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return FVector::ZeroVector;
	}

	if (bUseConstantVelocity)
	{
		return GetLocationAtDistanceAlongSpline(Time / Duration * GetSplineLength(), CoordinateSpace);
	}
	else
	{
		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
		const float TimeMultiplier = NumSegments / Duration;
		return GetLocationAtSplineInputKey(Time * TimeMultiplier, CoordinateSpace);
	}
}


FVector USplineComponent::GetDirectionAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return FVector::ZeroVector;
	}

	if (bUseConstantVelocity)
	{
		return GetDirectionAtDistanceAlongSpline(Time / Duration * GetSplineLength(), CoordinateSpace);
	}
	else
	{
		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
		const float TimeMultiplier = NumSegments / Duration;
		return GetDirectionAtSplineInputKey(Time * TimeMultiplier, CoordinateSpace);
	}
}


FVector USplineComponent::GetTangentAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return FVector::ZeroVector;
	}

	if (bUseConstantVelocity)
	{
		return GetTangentAtDistanceAlongSpline(Time / Duration * GetSplineLength(), CoordinateSpace);
	}
	else
	{
		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
		const float TimeMultiplier = NumSegments / Duration;
		return GetTangentAtSplineInputKey(Time * TimeMultiplier, CoordinateSpace);
	}
}


FRotator USplineComponent::GetRotationAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return FRotator::ZeroRotator;
	}

	if (bUseConstantVelocity)
	{
		return GetRotationAtDistanceAlongSpline(Time / Duration * GetSplineLength(), CoordinateSpace);
	}
	else
	{
		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
		const float TimeMultiplier = NumSegments / Duration;
		return GetRotationAtSplineInputKey(Time * TimeMultiplier, CoordinateSpace);
	}
}


FQuat USplineComponent::GetQuaternionAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return FQuat::Identity;
	}

	if (bUseConstantVelocity)
	{
		return GetQuaternionAtDistanceAlongSpline(Time / Duration * GetSplineLength(), CoordinateSpace);
	}
	else
	{
		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
		const float TimeMultiplier = NumSegments / Duration;
		return GetQuaternionAtSplineInputKey(Time * TimeMultiplier, CoordinateSpace);
	}
}


FVector USplineComponent::GetUpVectorAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return FVector::ZeroVector;
	}

	if (bUseConstantVelocity)
	{
		return GetUpVectorAtDistanceAlongSpline(Time / Duration * GetSplineLength(), CoordinateSpace);
	}
	else
	{
		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
		const float TimeMultiplier = NumSegments / Duration;
		return GetUpVectorAtSplineInputKey(Time * TimeMultiplier, CoordinateSpace);
	}
}


FVector USplineComponent::GetRightVectorAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return FVector::ZeroVector;
	}

	if (bUseConstantVelocity)
	{
		return GetRightVectorAtDistanceAlongSpline(Time / Duration * GetSplineLength(), CoordinateSpace);
	}
	else
	{
		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
		const float TimeMultiplier = NumSegments / Duration;
		return GetRightVectorAtSplineInputKey(Time * TimeMultiplier, CoordinateSpace);
	}
}


float USplineComponent::GetRollAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return 0.0f;
	}

	if (bUseConstantVelocity)
	{
		return GetRollAtDistanceAlongSpline(Time / Duration * GetSplineLength(), CoordinateSpace);
	}
	else
	{
		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
		const float TimeMultiplier = NumSegments / Duration;
		return GetRollAtSplineInputKey(Time * TimeMultiplier, CoordinateSpace);
	}
}


FTransform USplineComponent::GetTransformAtTime(float Time, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseConstantVelocity, bool bUseScale) const
{
	if (Duration == 0.0f)
	{
		return FTransform::Identity;
	}

	if (bUseConstantVelocity)
	{
		return GetTransformAtDistanceAlongSpline(Time / Duration * GetSplineLength(), CoordinateSpace, bUseScale);
	}
	else
	{
		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
		const float TimeMultiplier = NumSegments / Duration;
		return GetTransformAtSplineInputKey(Time * TimeMultiplier, CoordinateSpace, bUseScale);
	}
}


FVector USplineComponent::GetScaleAtTime(float Time, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return FVector(1.0f);
	}

	if (bUseConstantVelocity)
	{
		return GetScaleAtDistanceAlongSpline(Time / Duration * GetSplineLength());
	}
	else
	{
		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = bClosedLoop ? NumPoints : NumPoints - 1;
		const float TimeMultiplier = NumSegments / Duration;
		return GetScaleAtSplineInputKey(Time * TimeMultiplier);
	}
}


float USplineComponent::FindInputKeyClosestToWorldLocation(const FVector& WorldLocation) const
{
	const FVector LocalLocation = ComponentToWorld.InverseTransformPosition(WorldLocation);
	float Dummy;
	return SplineInfo.InaccurateFindNearest(LocalLocation, Dummy);
}


FVector USplineComponent::FindLocationClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = FindInputKeyClosestToWorldLocation(WorldLocation);
	return GetLocationAtSplineInputKey(Param, CoordinateSpace);
}


FVector USplineComponent::FindDirectionClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = FindInputKeyClosestToWorldLocation(WorldLocation);
	return GetDirectionAtSplineInputKey(Param, CoordinateSpace);
}


FVector USplineComponent::FindTangentClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = FindInputKeyClosestToWorldLocation(WorldLocation);
	return GetTangentAtSplineInputKey(Param, CoordinateSpace);
}


FQuat USplineComponent::FindQuaternionClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = FindInputKeyClosestToWorldLocation(WorldLocation);
	return GetQuaternionAtSplineInputKey(Param, CoordinateSpace);
}


FRotator USplineComponent::FindRotationClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = FindInputKeyClosestToWorldLocation(WorldLocation);
	return GetRotationAtSplineInputKey(Param, CoordinateSpace);
}


FVector USplineComponent::FindUpVectorClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = FindInputKeyClosestToWorldLocation(WorldLocation);
	return GetUpVectorAtSplineInputKey(Param, CoordinateSpace);
}


FVector USplineComponent::FindRightVectorClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = FindInputKeyClosestToWorldLocation(WorldLocation);
	return GetRightVectorAtSplineInputKey(Param, CoordinateSpace);
}


float USplineComponent::FindRollClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace) const
{
	const float Param = FindInputKeyClosestToWorldLocation(WorldLocation);
	return GetRollAtSplineInputKey(Param, CoordinateSpace);
}


FVector USplineComponent::FindScaleClosestToWorldLocation(const FVector& WorldLocation) const
{
	const float Param = FindInputKeyClosestToWorldLocation(WorldLocation);
	return GetScaleAtSplineInputKey(Param);
}


FTransform USplineComponent::FindTransformClosestToWorldLocation(const FVector& WorldLocation, ESplineCoordinateSpace::Type CoordinateSpace, bool bUseScale) const
{
	const float Param = FindInputKeyClosestToWorldLocation(WorldLocation);
	return GetTransformAtSplineInputKey(Param, CoordinateSpace, bUseScale);
}


/** Used to store spline data during RerunConstructionScripts */
class FSplineInstanceData : public FSceneComponentInstanceData
{
public:
	explicit FSplineInstanceData(const USplineComponent* SourceComponent)
		: FSceneComponentInstanceData(SourceComponent)
	{
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<USplineComponent>(Component)->ApplyComponentInstanceData(this, (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript));
	}

	FInterpCurveVector SplineInfo;
	FInterpCurveQuat SplineRotInfo;
	FInterpCurveVector SplineScaleInfo;
	bool bSplineHasBeenEdited;
};


FActorComponentInstanceData* USplineComponent::GetComponentInstanceData() const
{
	FSplineInstanceData* SplineInstanceData = new FSplineInstanceData(this);
	if (bSplineHasBeenEdited)
	{
		SplineInstanceData->SplineInfo = SplineInfo;
		SplineInstanceData->SplineRotInfo = SplineRotInfo;
		SplineInstanceData->SplineScaleInfo = SplineScaleInfo;
	}
	SplineInstanceData->bSplineHasBeenEdited = bSplineHasBeenEdited;

	return SplineInstanceData;
}


void USplineComponent::ApplyComponentInstanceData(FSplineInstanceData* SplineInstanceData, const bool bPostUCS)
{
	check(SplineInstanceData);

	if (SplineInstanceData->bSplineHasBeenEdited)
	{
		SplineInfo = SplineInstanceData->SplineInfo;
		SplineRotInfo = SplineInstanceData->SplineRotInfo;
		SplineScaleInfo = SplineInstanceData->SplineScaleInfo;
	}

	bSplineHasBeenEdited = SplineInstanceData->bSplineHasBeenEdited;

	UpdateSpline();
}


#if WITH_EDITOR
void USplineComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		static const FName ReparamStepsPerSegmentName = GET_MEMBER_NAME_CHECKED(USplineComponent, ReparamStepsPerSegment);
		static const FName StationaryEndpointsName = GET_MEMBER_NAME_CHECKED(USplineComponent, bStationaryEndpoints);
		static const FName DefaultUpVectorName = GET_MEMBER_NAME_CHECKED(USplineComponent, DefaultUpVector);
		static const FName ClosedLoopName = GET_MEMBER_NAME_CHECKED(USplineComponent, bClosedLoop);
		static const FName SplineInfoName = GET_MEMBER_NAME_CHECKED(USplineComponent, SplineInfo);
		static const FName SplineHasBeenEditedName = GET_MEMBER_NAME_CHECKED(USplineComponent, bSplineHasBeenEdited);

		const FName PropertyName(PropertyChangedEvent.Property->GetFName());
		if (PropertyName == ReparamStepsPerSegmentName ||
			PropertyName == StationaryEndpointsName ||
			PropertyName == DefaultUpVectorName ||
			PropertyName == ClosedLoopName)
		{
			UpdateSpline();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
