// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvQueryTraceHelpers.h"


template<>
void FEQSHelpers::FBatchTrace::DoSingleSourceMultiDestinations<EEnvTraceShape::Line>(const FVector& Source, TArray<FNavLocation>& Points)
{
	FVector HitPos(FVector::ZeroVector);
	for (int32 Idx = Points.Num() - 1; Idx >= 0; Idx--)
	{
		const bool bHit = RunLineTrace(Source, Points[Idx].Location, HitPos);
		if (bHit)
		{
			Points[Idx] = FNavLocation(HitPos);
		}
		else if (TraceMode == ETraceMode::Discard)
		{
			Points.RemoveAt(Idx, 1, false);
		}
	}
}


template<>
void FEQSHelpers::FBatchTrace::DoSingleSourceMultiDestinations<EEnvTraceShape::Box>(const FVector& Source, TArray<FNavLocation>& Points)
{
	FVector HitPos(FVector::ZeroVector);
	for (int32 Idx = Points.Num() - 1; Idx >= 0; Idx--)
	{
		const bool bHit = RunBoxTrace(Source, Points[Idx].Location, HitPos);
		if (bHit)
		{
			Points[Idx] = FNavLocation(HitPos);
		}
		else if (TraceMode == ETraceMode::Discard)
		{
			Points.RemoveAt(Idx, 1, false);
		}
	}
}


template<>
void FEQSHelpers::FBatchTrace::DoSingleSourceMultiDestinations<EEnvTraceShape::Sphere>(const FVector& Source, TArray<FNavLocation>& Points)
{
	FVector HitPos(FVector::ZeroVector);
	for (int32 Idx = Points.Num() - 1; Idx >= 0; Idx--)
	{
		const bool bHit = RunSphereTrace(Source, Points[Idx].Location, HitPos);
		if (bHit)
		{
			Points[Idx] = FNavLocation(HitPos);
		}
		else if (TraceMode == ETraceMode::Discard)
		{
			Points.RemoveAt(Idx, 1, false);
		}
	}
}


template<>
void FEQSHelpers::FBatchTrace::DoSingleSourceMultiDestinations<EEnvTraceShape::Capsule>(const FVector& Source, TArray<FNavLocation>& Points)
{
	FVector HitPos(FVector::ZeroVector);
	for (int32 Idx = Points.Num() - 1; Idx >= 0; Idx--)
	{
		const bool bHit = RunCapsuleTrace(Source, Points[Idx].Location, HitPos);
		if (bHit)
		{
			Points[Idx] = FNavLocation(HitPos);
		}
		else if (TraceMode == ETraceMode::Discard)
		{
			Points.RemoveAt(Idx, 1, false);
		}
	}
}


template<>
void FEQSHelpers::FBatchTrace::DoProject<EEnvTraceShape::Line>(TArray<FNavLocation>& Points, float StartOffsetZ, float EndOffsetZ, float HitOffsetZ)
{
	FVector HitPos(FVector::ZeroVector);
	for (int32 Idx = Points.Num() - 1; Idx >= 0; Idx--)
	{
		const bool bHit = RunLineTrace(Points[Idx].Location + FVector(0, 0, StartOffsetZ), Points[Idx].Location + FVector(0, 0, EndOffsetZ), HitPos);

		if (bHit)
		{
			Points[Idx] = FNavLocation(HitPos + FVector(0, 0, HitOffsetZ));
		}
		else if (TraceMode == ETraceMode::Discard)
		{
			Points.RemoveAt(Idx, 1, false);
		}

		if (TraceHits.IsValidIndex(Idx))
		{
			TraceHits[Idx] = bHit ? 1 : 0;
		}
	}
}


template<>
void FEQSHelpers::FBatchTrace::DoProject<EEnvTraceShape::Box>(TArray<FNavLocation>& Points, float StartOffsetZ, float EndOffsetZ, float HitOffsetZ)
{
	FVector HitPos(FVector::ZeroVector);
	for (int32 Idx = Points.Num() - 1; Idx >= 0; Idx--)
	{
		const bool bHit = RunBoxTrace(Points[Idx].Location + FVector(0, 0, StartOffsetZ), Points[Idx].Location + FVector(0, 0, EndOffsetZ), HitPos);
		if (bHit)
		{
			Points[Idx] = FNavLocation(HitPos + FVector(0, 0, HitOffsetZ));
		}
		else if (TraceMode == ETraceMode::Discard)
		{
			Points.RemoveAt(Idx, 1, false);
		}

		if (TraceHits.IsValidIndex(Idx))
		{
			TraceHits[Idx] = bHit ? 1 : 0;
		}
	}
}


template<>
void FEQSHelpers::FBatchTrace::DoProject<EEnvTraceShape::Sphere>(TArray<FNavLocation>& Points, float StartOffsetZ, float EndOffsetZ, float HitOffsetZ)
{
	FVector HitPos(FVector::ZeroVector);
	for (int32 Idx = Points.Num() - 1; Idx >= 0; Idx--)
	{
		const bool bHit = RunSphereTrace(Points[Idx].Location + FVector(0, 0, StartOffsetZ), Points[Idx].Location + FVector(0, 0, EndOffsetZ), HitPos);
		if (bHit)
		{
			Points[Idx] = FNavLocation(HitPos + FVector(0, 0, HitOffsetZ));
		}
		else if (TraceMode == ETraceMode::Discard)
		{
			Points.RemoveAt(Idx, 1, false);
		}

		if (TraceHits.IsValidIndex(Idx))
		{
			TraceHits[Idx] = bHit ? 1 : 0;
		}
	}
}


template<>
void FEQSHelpers::FBatchTrace::DoProject<EEnvTraceShape::Capsule>(TArray<FNavLocation>& Points, float StartOffsetZ, float EndOffsetZ, float HitOffsetZ)
{
	FVector HitPos(FVector::ZeroVector);
	for (int32 Idx = Points.Num() - 1; Idx >= 0; Idx--)
	{
		const bool bHit = RunCapsuleTrace(Points[Idx].Location + FVector(0, 0, StartOffsetZ), Points[Idx].Location + FVector(0, 0, EndOffsetZ), HitPos);
		if (bHit)
		{
			Points[Idx] = FNavLocation(HitPos + FVector(0, 0, HitOffsetZ));
		}
		else if (TraceMode == ETraceMode::Discard)
		{
			Points.RemoveAt(Idx, 1, false);
		}

		if (TraceHits.IsValidIndex(Idx))
		{
			TraceHits[Idx] = bHit ? 1 : 0;
		}
	}
}


void FEQSHelpers::RunNavRaycasts(const ANavigationData& NavData, const UObject& Querier, const FEnvTraceData& TraceData, const FVector& SourcePt, TArray<FNavLocation>& Points, ETraceMode TraceMode /*= ETraceMode::Keep*/)
{
	FSharedConstNavQueryFilter NavigationFilter = UNavigationQueryFilter::GetQueryFilter(NavData, &Querier, TraceData.NavigationFilter);

	TArray<FNavigationRaycastWork> RaycastWorkload;
	RaycastWorkload.Reserve(Points.Num());

	for (const auto& ItemLocation : Points)
	{
		RaycastWorkload.Add(FNavigationRaycastWork(SourcePt, ItemLocation.Location));
	}

	NavData.BatchRaycast(RaycastWorkload, NavigationFilter);

	for (int32 Idx = 0; Idx < Points.Num(); Idx++)
	{
		Points[Idx] = RaycastWorkload[Idx].HitLocation;
	}

	if (TraceMode == ETraceMode::Discard)
	{
		for (int32 Idx = Points.Num() - 1; Idx >= 0; --Idx)
		{
			if (!RaycastWorkload[Idx].bDidHit)
			{
				Points.RemoveAt(Idx, 1, false);
			}
		}
	}
}


void FEQSHelpers::RunNavProjection(const ANavigationData& NavData, const UObject& Querier, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, ETraceMode TraceMode /*= ETraceMode::Discard*/)
{
	FSharedConstNavQueryFilter NavigationFilter = UNavigationQueryFilter::GetQueryFilter(NavData, &Querier, TraceData.NavigationFilter);
	TArray<FNavigationProjectionWork> Workload;
	Workload.Reserve(Points.Num());

	if (TraceData.ProjectDown == TraceData.ProjectUp)
	{
		for (const auto& Point : Points)
		{
			Workload.Add(FNavigationProjectionWork(Point.Location));
		}
	}
	else
	{
		const FVector VerticalOffset = FVector(0, 0, (TraceData.ProjectUp - TraceData.ProjectDown) / 2);
		for (const auto& Point : Points)
		{
			Workload.Add(FNavigationProjectionWork(Point.Location + VerticalOffset));
		}
	}

	const FVector ProjectionExtent(TraceData.ExtentX, TraceData.ExtentX, (TraceData.ProjectDown + TraceData.ProjectUp) / 2);
	NavData.BatchProjectPoints(Workload, ProjectionExtent, NavigationFilter);

	for (int32 Idx = Workload.Num() - 1; Idx >= 0; Idx--)
	{
		if (Workload[Idx].bResult)
		{
			Points[Idx] = Workload[Idx].OutLocation;
			Points[Idx].Location.Z += TraceData.PostProjectionVerticalOffset;
		}
		else if (TraceMode == ETraceMode::Discard)
		{
			Points.RemoveAt(Idx, 1, false);
		}
	}
}


void FEQSHelpers::RunPhysRaycasts(UWorld* World, const FEnvTraceData& TraceData, const FVector& SourcePt, TArray<FNavLocation>& Points, const TArray<AActor*>& IgnoredActors, ETraceMode TraceMode)
{
	ECollisionChannel TraceCollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceData.TraceChannel);
	FVector TraceExtent(TraceData.ExtentX, TraceData.ExtentY, TraceData.ExtentZ);

	FCollisionQueryParams TraceParams(TEXT("EnvQueryTrace"), TraceData.bTraceComplex);
	TraceParams.bTraceAsyncScene = true;
	TraceParams.AddIgnoredActors(IgnoredActors);

	FBatchTrace BatchOb(World, TraceCollisionChannel, TraceParams, TraceExtent, TraceMode);

	switch (TraceData.TraceShape)
	{
	case EEnvTraceShape::Line:
		BatchOb.DoSingleSourceMultiDestinations<EEnvTraceShape::Line>(SourcePt, Points);
		break;

	case EEnvTraceShape::Sphere:
		BatchOb.DoSingleSourceMultiDestinations<EEnvTraceShape::Sphere>(SourcePt, Points);
		break;

	case EEnvTraceShape::Capsule:
		BatchOb.DoSingleSourceMultiDestinations<EEnvTraceShape::Capsule>(SourcePt, Points);
		break;

	case EEnvTraceShape::Box:
		BatchOb.DoSingleSourceMultiDestinations<EEnvTraceShape::Box>(SourcePt, Points);
		break;

	default:
		break;
	}
}


void FEQSHelpers::RunPhysProjection(UWorld* World, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, ETraceMode TraceMode)
{
	ECollisionChannel TraceCollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceData.TraceChannel);
	FVector TraceExtent(TraceData.ExtentX, TraceData.ExtentY, TraceData.ExtentZ);

	FCollisionQueryParams TraceParams(TEXT("EnvQueryTrace"), TraceData.bTraceComplex);
	TraceParams.bTraceAsyncScene = true;

	FBatchTrace BatchOb(World, TraceCollisionChannel, TraceParams, TraceExtent, TraceMode);

	switch (TraceData.TraceShape)
	{
	case EEnvTraceShape::Line:
		BatchOb.DoProject<EEnvTraceShape::Line>(Points, TraceData.ProjectUp, -TraceData.ProjectDown, TraceData.PostProjectionVerticalOffset);
		break;

	case EEnvTraceShape::Sphere:
		BatchOb.DoProject<EEnvTraceShape::Sphere>(Points, TraceData.ProjectUp, -TraceData.ProjectDown, TraceData.PostProjectionVerticalOffset);
		break;

	case EEnvTraceShape::Capsule:
		BatchOb.DoProject<EEnvTraceShape::Capsule>(Points, TraceData.ProjectUp, -TraceData.ProjectDown, TraceData.PostProjectionVerticalOffset);
		break;

	case EEnvTraceShape::Box:
		BatchOb.DoProject<EEnvTraceShape::Box>(Points, TraceData.ProjectUp, -TraceData.ProjectDown, TraceData.PostProjectionVerticalOffset);
		break;

	default:
		break;
	}
}

void FEQSHelpers::RunPhysProjection(UWorld* World, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, TArray<uint8>& TraceHits)
{
	ECollisionChannel TraceCollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceData.TraceChannel);
	FVector TraceExtent(TraceData.ExtentX, TraceData.ExtentY, TraceData.ExtentZ);

	FCollisionQueryParams TraceParams(TEXT("EnvQueryTrace"), TraceData.bTraceComplex);
	TraceParams.bTraceAsyncScene = true;

	FBatchTrace BatchOb(World, TraceCollisionChannel, TraceParams, TraceExtent, ETraceMode::Keep);
	BatchOb.TraceHits.AddZeroed(Points.Num());

	switch (TraceData.TraceShape)
	{
	case EEnvTraceShape::Line:
		BatchOb.DoProject<EEnvTraceShape::Line>(Points, TraceData.ProjectUp, -TraceData.ProjectDown, TraceData.PostProjectionVerticalOffset);
		break;

	case EEnvTraceShape::Sphere:
		BatchOb.DoProject<EEnvTraceShape::Sphere>(Points, TraceData.ProjectUp, -TraceData.ProjectDown, TraceData.PostProjectionVerticalOffset);
		break;

	case EEnvTraceShape::Capsule:
		BatchOb.DoProject<EEnvTraceShape::Capsule>(Points, TraceData.ProjectUp, -TraceData.ProjectDown, TraceData.PostProjectionVerticalOffset);
		break;

	case EEnvTraceShape::Box:
		BatchOb.DoProject<EEnvTraceShape::Box>(Points, TraceData.ProjectUp, -TraceData.ProjectDown, TraceData.PostProjectionVerticalOffset);
		break;

	default:
		break;
	}

	TraceHits.Append(BatchOb.TraceHits);
}

void FEQSHelpers::RunNavRaycasts(const ANavigationData& NavData, const FEnvTraceData& TraceData, const FVector& SourcePt, TArray<FNavLocation>& Points, ETraceMode TraceMode)
{
	RunNavRaycasts(NavData, NavData, TraceData, SourcePt, Points, TraceMode);
}

void FEQSHelpers::RunNavProjection(const ANavigationData& NavData, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, ETraceMode TraceMode)
{
	RunNavProjection(NavData, NavData, TraceData, Points, TraceMode);
}
