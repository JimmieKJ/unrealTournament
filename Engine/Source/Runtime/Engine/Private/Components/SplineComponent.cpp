// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Spline.cpp
=============================================================================*/

#include "EnginePrivate.h"
#include "Components/SplineComponent.h"
#include "ComponentInstanceDataCache.h"


USplineComponent::USplineComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAllowSplineEditingPerInstance(true)
	, ReparamStepsPerSegment(10)
	, Duration(1.0f)
	, bStationaryEndpoints(false)
	, bClosedLoop(false)
#if WITH_EDITORONLY_DATA
	, EditorUnselectedSplineSegmentColor(FLinearColor(1.0f, 1.0f, 1.0f))
	, EditorSelectedSplineSegmentColor(FLinearColor(1.0f, 0.0f, 0.0f))
#endif
{
	SplineInfo.Points.Reset(10);

	// Add 2 keys by default
	int32 PointIndex = SplineInfo.AddPoint(0.f, FVector(0,0,0));
	SplineInfo.Points[PointIndex].InterpMode = CIM_CurveAuto;

	PointIndex = SplineInfo.AddPoint(1.f, FVector(100,0,0));
	SplineInfo.Points[PointIndex].InterpMode = CIM_CurveAuto;

	UpdateSpline();
}


void USplineComponent::PostLoad()
{
	Super::PostLoad();
	UpdateSpline();
}


void USplineComponent::PostEditImport()
{
	Super::PostEditImport();
	UpdateSpline();
}


void USplineComponent::UpdateSpline()
{
	const int32 NumPoints = SplineInfo.Points.Num();
	check(!bClosedLoop || NumPoints == 0 || (NumPoints >= 2 && SplineInfo.Points[0].OutVal == SplineInfo.Points[NumPoints - 1].OutVal));

	// Automatically set the tangents on any CurveAuto keys
	SplineInfo.AutoSetTangents(0.0f, bStationaryEndpoints);

	// Nothing else to do if less than 2 points
	if (NumPoints < 2)
	{
		return;
	}

	// Adjust auto tangents for first and last keys to take into account the looping
	if (bClosedLoop)
	{
		auto& FirstPoint = SplineInfo.Points[0];
		auto& LastPoint = SplineInfo.Points[NumPoints - 1];
		const auto& SecondPoint = SplineInfo.Points[1];
		const auto& PenultimatePoint = SplineInfo.Points[NumPoints - 2];

		if (FirstPoint.InterpMode == CIM_CurveAuto || FirstPoint.InterpMode == CIM_CurveAutoClamped)
		{
			FVector Tangent;
			ComputeCurveTangent(
				PenultimatePoint.InVal - LastPoint.InVal, PenultimatePoint.OutVal,
				FirstPoint.InVal, FirstPoint.OutVal,
				SecondPoint.InVal, SecondPoint.OutVal,
				0.0f,
				FirstPoint.InterpMode == CIM_CurveAutoClamped,
				Tangent);

			FirstPoint.LeaveTangent = Tangent;
			FirstPoint.ArriveTangent = Tangent;
			LastPoint.LeaveTangent = Tangent;
			LastPoint.ArriveTangent = Tangent;
		}
	}

	const int32 NumSegments = NumPoints - 1;

	// Start by clearing it
	SplineReparamTable.Points.Reset(NumSegments * ReparamStepsPerSegment + 1);
	float AccumulatedLength = 0.0f;
	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		for (int32 Step = 0; Step < ReparamStepsPerSegment; ++Step)
		{
			const float Param = static_cast<float>(Step) / ReparamStepsPerSegment;
			const float SegmentLength = (Step == 0) ? 0.0f : GetSegmentLength(SegmentIndex, Param);
			SplineReparamTable.AddPoint(SegmentLength + AccumulatedLength, SegmentIndex + Param);
		}
		AccumulatedLength += GetSegmentLength(SegmentIndex, 1.0f);
	}
	SplineReparamTable.AddPoint(AccumulatedLength, static_cast<float>(NumSegments));
}


float USplineComponent::GetSegmentLength(const int32 Index, const float Param) const
{
	check(Index < SplineInfo.Points.Num() - 1);
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
	const auto& EndPoint = SplineInfo.Points[Index + 1];
	check(static_cast<int32>(EndPoint.InVal) - static_cast<int32>(StartPoint.InVal) == 1);

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
		const FVector Derivative = (Coeff1 * Alpha + Coeff2) * Alpha + Coeff3;
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

	check(Index < SplineInfo.Points.Num() - 1);
	check(Length >= 0.0f && Length <= SegmentLength);
	check(static_cast<int32>(SplineInfo.Points[Index + 1].InVal) - static_cast<int32>(SplineInfo.Points[Index].InVal) == 1);

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


void USplineComponent::SetClosedLoop(bool bInClosedLoop)
{
	UpdateLoopEndpoint(bInClosedLoop);
	UpdateSpline();
}


void USplineComponent::UpdateLoopEndpoint(bool bInClosedLoop)
{
	if (bClosedLoop != bInClosedLoop)
	{
		bClosedLoop = bInClosedLoop;

		if (bClosedLoop)
		{
			AddLoopEndpoint();
		}
		else
		{
			RemoveLoopEndpoint();
		}
	}
}


bool USplineComponent::IsClosedLoop() const
{
	return bClosedLoop;
}


void USplineComponent::RemoveLoopEndpoint()
{
	const int32 NumPoints = SplineInfo.Points.Num();
	check(!bClosedLoop);
	check(NumPoints == 0 || (NumPoints >= 2 && SplineInfo.Points[0].OutVal == SplineInfo.Points[NumPoints - 1].OutVal));
	if (NumPoints > 0)
	{
		SplineInfo.Points.RemoveAt(NumPoints - 1, 1, false);
	}
}


void USplineComponent::AddLoopEndpoint()
{
	check(bClosedLoop);
	const int32 NumPoints = SplineInfo.Points.Num();
	if (NumPoints > 0)
	{
		FInterpCurvePoint<FVector> EndPoint(SplineInfo.Points[0]);
		EndPoint.InVal = static_cast<float>(NumPoints);
		SplineInfo.Points.Add(EndPoint);
	}
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
	SplineInfo.Reset();
	SplineReparamTable.Reset();
}


void USplineComponent::AddSplineWorldPoint(const FVector& Position)
{
	// If it's a closed loop, remove the endpoint before adding a new point
	const bool bWasLoop = IsClosedLoop();
	UpdateLoopEndpoint(false);

	float InputKey = static_cast<float>(SplineInfo.Points.Num());
	const int32 PointIndex = SplineInfo.AddPoint(InputKey, ComponentToWorld.InverseTransformPosition(Position));
	SplineInfo.Points[PointIndex].InterpMode = CIM_CurveAuto;

	// Then re-close the spline if required
	UpdateLoopEndpoint(bWasLoop);

	UpdateSpline();
}


void USplineComponent::AddSplineLocalPoint(const FVector& Position)
{
	// If it's a closed loop, remove the endpoint before adding a new point
	const bool bWasLoop = IsClosedLoop();
	UpdateLoopEndpoint(false);

	float InputKey = static_cast<float>(SplineInfo.Points.Num());
	const int32 PointIndex = SplineInfo.AddPoint(InputKey, Position);
	SplineInfo.Points[PointIndex].InterpMode = CIM_CurveAuto;

	// Then re-close the spline if required
	UpdateLoopEndpoint(bWasLoop);

	UpdateSpline();
}


void USplineComponent::SetSplineWorldPoints(const TArray<FVector>& Points)
{
	// If it's a closed loop, mark it as not closed before setting a new array of points
	const bool bWasLoop = IsClosedLoop();
	UpdateLoopEndpoint(false);

	SplineInfo.Points.Reset(bWasLoop ? Points.Num() + 1 : Points.Num());
	float InputKey = 0.0f;
	for (const auto& Point : Points)
	{
		int32 PointIndex = SplineInfo.AddPoint(InputKey, ComponentToWorld.InverseTransformPosition(Point));
		SplineInfo.Points[PointIndex].InterpMode = CIM_CurveAuto;
		InputKey += 1.0f;
	}

	UpdateLoopEndpoint(bWasLoop);

	UpdateSpline();
}


void USplineComponent::SetSplineLocalPoints(const TArray<FVector>& Points)
{
	const bool bWasLoop = IsClosedLoop();
	UpdateLoopEndpoint(false);

	SplineInfo.Points.Reset(bWasLoop ? Points.Num() + 1 : Points.Num());
	float InputKey = 0.0f;
	for (const auto& Point : Points)
	{
		int32 PointIndex = SplineInfo.AddPoint(InputKey, Point);
		SplineInfo.Points[PointIndex].InterpMode = CIM_CurveAuto;
		InputKey += 1.0f;
	}

	UpdateLoopEndpoint(bWasLoop);

	UpdateSpline();
}


void USplineComponent::SetWorldLocationAtSplinePoint(int32 PointIndex, const FVector& InLocation)
{
	const int32 NumPoints = SplineInfo.Points.Num();

	if ((PointIndex >= 0) && (PointIndex < NumPoints))
	{
		SplineInfo.Points[PointIndex].OutVal = ComponentToWorld.InverseTransformPosition(InLocation);

		if (IsClosedLoop())
		{
			// In a closed loop, the first and last points are tied, so update one with the other
			if (PointIndex == 0)
			{
				SplineInfo.Points[NumPoints - 1].OutVal = InLocation;
			}
			else if (PointIndex == NumPoints - 1)
			{
				SplineInfo.Points[0].OutVal = InLocation;
			}
		}

		UpdateSpline();
	}
}


ESplinePointType::Type USplineComponent::GetSplinePointType(int32 PointIndex) const
{
	if ((PointIndex >= 0) && (PointIndex < SplineInfo.Points.Num()))
	{
		switch (SplineInfo.Points[PointIndex].InterpMode)
		{
			case CIM_CurveAuto:			return ESplinePointType::Curve;
			case CIM_CurveAutoClamped:	return ESplinePointType::CurveClamped;
			case CIM_Linear:			return ESplinePointType::Linear;
		}
	}

	return ESplinePointType::Constant;
}


void USplineComponent::SetSplinePointType(int32 PointIndex, ESplinePointType::Type Type)
{
	EInterpCurveMode InterpMode;
	switch (Type)
	{
		case ESplinePointType::Curve:			InterpMode = CIM_CurveAuto; break;
		case ESplinePointType::CurveClamped:	InterpMode = CIM_CurveAutoClamped; break;
		case ESplinePointType::Linear:			InterpMode = CIM_Linear; break;
		default:								InterpMode = CIM_Constant;
	}

	const int32 NumPoints = SplineInfo.Points.Num();

	if ((PointIndex >= 0) && (PointIndex < NumPoints))
	{
		SplineInfo.Points[PointIndex].InterpMode = InterpMode;

		if (IsClosedLoop())
		{
			// In a closed loop, the first and last points are tied, so update one with the other
			if (PointIndex == 0)
			{
				SplineInfo.Points[NumPoints - 1].InterpMode = InterpMode;
			}
			else if (PointIndex == NumPoints - 1)
			{
				SplineInfo.Points[0].InterpMode = InterpMode;
			}
		}

		UpdateSpline();
	}
}


int32 USplineComponent::GetNumSplinePoints() const
{
	return SplineInfo.Points.Num();
}

FVector USplineComponent::GetWorldLocationAtSplinePoint(int32 PointIndex) const
{
	FVector LocalLocation(0,0,0);
	if ((PointIndex >= 0) && (PointIndex < SplineInfo.Points.Num()))
	{
		LocalLocation = SplineInfo.Points[PointIndex].OutVal;
	}
	return ComponentToWorld.TransformPosition(LocalLocation);
}

void USplineComponent::GetLocalLocationAndTangentAtSplinePoint(int32 PointIndex, FVector& LocalLocation, FVector& LocalTangent) const
{
	LocalTangent = FVector(0, 0, 0);
	LocalLocation = FVector(0, 0, 0);
	if ((PointIndex >= 0) && (PointIndex < SplineInfo.Points.Num()))
	{
		LocalTangent = SplineInfo.Points[PointIndex].LeaveTangent;
		LocalLocation = SplineInfo.Points[PointIndex].OutVal;
	}
}

float USplineComponent::GetDistanceAlongSplineAtSplinePoint(int32 PointIndex) const
{
	if ((PointIndex >= 0) && (PointIndex < SplineInfo.Points.Num()))
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
		return SplineReparamTable.Points[NumPoints - 1].InVal;
	}
	
	return 0.f;
}


float USplineComponent::GetInputKeyAtDistanceAlongSpline(float Distance) const
{
	const int32 NumPoints = SplineInfo.Points.Num();

	if (NumPoints < 2)
	{
		return 0.0f;
	}

	const float TimeMultiplier = Duration / (NumPoints - 1.0f);
	return SplineReparamTable.Eval(Distance, 0.f) * TimeMultiplier;
}


FVector USplineComponent::GetWorldLocationAtDistanceAlongSpline(float Distance) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.f);
	const FVector Location = SplineInfo.Eval(Param, FVector::ZeroVector);
	return ComponentToWorld.TransformPosition(Location);
}


FVector USplineComponent::GetWorldTangentAtDistanceAlongSpline(float Distance) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.f);
	const FVector Tangent = SplineInfo.EvalDerivative(Param, FVector::ZeroVector);
	return ComponentToWorld.TransformVector(Tangent);
}


FVector USplineComponent::GetWorldDirectionAtDistanceAlongSpline(float Distance) const
{
	const float Param = SplineReparamTable.Eval(Distance, 0.f);
	const FVector Tangent = SplineInfo.EvalDerivative(Param, FVector::ZeroVector).GetSafeNormal();
	return ComponentToWorld.TransformVectorNoScale(Tangent);
}


FRotator USplineComponent::GetWorldRotationAtDistanceAlongSpline(float Distance) const
{
	const FVector Dir = GetWorldDirectionAtDistanceAlongSpline(Distance);
	return Dir.Rotation();
}


FVector USplineComponent::GetWorldLocationAtTime(float Time, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return FVector::ZeroVector;
	}

	if (bUseConstantVelocity)
	{
		return GetWorldLocationAtDistanceAlongSpline(Time / Duration * GetSplineLength());
	}

	const float TimeMultiplier = (SplineInfo.Points.Num() - 1.0f) / Duration;
	const FVector Location = SplineInfo.Eval(Time * TimeMultiplier, FVector::ZeroVector);
	return ComponentToWorld.TransformPosition(Location);
}


FVector USplineComponent::GetWorldDirectionAtTime(float Time, bool bUseConstantVelocity) const
{
	if (Duration == 0.0f)
	{
		return FVector::ZeroVector;
	}

	if (bUseConstantVelocity)
	{
		return GetWorldDirectionAtDistanceAlongSpline(Time / Duration * GetSplineLength());
	}

	const float TimeMultiplier = (SplineInfo.Points.Num() - 1.0f) / Duration;
	const FVector Tangent = SplineInfo.EvalDerivative(Time * TimeMultiplier, FVector::ZeroVector).GetSafeNormal();
	return ComponentToWorld.TransformVectorNoScale(Tangent);
}


FRotator USplineComponent::GetWorldRotationAtTime(float Time, bool bUseConstantVelocity) const
{
	FVector WorldDir = GetWorldDirectionAtTime(Time, bUseConstantVelocity);
	return WorldDir.Rotation();
}


void USplineComponent::RefreshSplineInputs()
{
	for(int32 KeyIdx=0; KeyIdx<SplineInfo.Points.Num(); KeyIdx++)
	{
		SplineInfo.Points[KeyIdx].InVal = KeyIdx;
	}
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
		CastChecked<USplineComponent>(Component)->ApplyComponentInstanceData(this);
	}

	FInterpCurveVector SplineInfo;
	bool bClosedLoop;
};

FName USplineComponent::GetComponentInstanceDataType() const
{
	static const FName SplineInstanceDataTypeName(TEXT("SplineInstanceData"));
	return SplineInstanceDataTypeName;
}

FActorComponentInstanceData* USplineComponent::GetComponentInstanceData() const
{
	FActorComponentInstanceData* InstanceData = nullptr;
	if (bAllowSplineEditingPerInstance)
	{
		FSplineInstanceData* SplineInstanceData = new FSplineInstanceData(this);
		SplineInstanceData->SplineInfo = SplineInfo;
		SplineInstanceData->bClosedLoop = bClosedLoop;

		InstanceData = SplineInstanceData;
	}
	else
	{
		InstanceData = Super::GetComponentInstanceData();
	}
	return InstanceData;
}

void USplineComponent::ApplyComponentInstanceData(FSplineInstanceData* SplineInstanceData)
{
	if (SplineInstanceData)
	{
		if (bAllowSplineEditingPerInstance)
		{
			SplineInfo = SplineInstanceData->SplineInfo;

			// If the construction script changed bClosedLoop, amend the applied points accordingly
			if (SplineInstanceData->bClosedLoop != bClosedLoop)
			{
				if (bClosedLoop)
				{
					AddLoopEndpoint();
				}
				else
				{
					RemoveLoopEndpoint();
				}
			}

			UpdateSpline();
		}
	}
}

#if WITH_EDITOR
void USplineComponent::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange != nullptr && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(USplineComponent, bClosedLoop))
	{
		if (bClosedLoop)
		{
			// We need to detect changes in state in order to correctly open or close the loop.
			// Since we don't know what state is about to be set, we ensure that the spline is marked as not closed here, so that either:
			//  a) Nothing needs to be done if the property is set to false, or
			//  b) The spline is already set as not looped, so that setting the property to true will work correctly in PostEditChangeProperty.
			bClosedLoop = false;
			RemoveLoopEndpoint();
		}
	}
}

void USplineComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent)
{
	if (PropertyChangedChainEvent.Property != nullptr)
	{
		const FName PropertyName(PropertyChangedChainEvent.Property->GetFName());
		if (PropertyName == GET_MEMBER_NAME_CHECKED(USplineComponent, bClosedLoop))
		{
			if (bClosedLoop)
			{
				// Spline is guaranteed to be non-looping when we get here (due to PreEditChange).
				// Now just force the loop endpoint to be added if bClosedLoop == true.
				AddLoopEndpoint();
			}

			UpdateSpline();
		}
	}

	Super::PostEditChangeChainProperty(PropertyChangedChainEvent);
}

void USplineComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		const FName PropertyName(PropertyChangedEvent.Property->GetFName());
		if (PropertyName == GET_MEMBER_NAME_CHECKED(USplineComponent, ReparamStepsPerSegment) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(USplineComponent, bStationaryEndpoints))
		{
			UpdateSpline();
		}

		if (PropertyName == GET_MEMBER_NAME_CHECKED(USplineComponent, bClosedLoop))
		{
			if (bClosedLoop)
			{
				// Spline is guaranteed to be non-looping when we get here (due to PreEditChange).
				// Now just force the loop endpoint to be added if bClosedLoop == true.
				AddLoopEndpoint();
			}

			UpdateSpline();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
