// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"

UEnvQueryGenerator_ProjectedPoints::UEnvQueryGenerator_ProjectedPoints(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ProjectionData.SetNavmeshOnly();
	ProjectionData.bCanProjectDown = true;
	ProjectionData.bCanDisableTrace = true;
	ProjectionData.ExtentX = 0.0f;
}

void UEnvQueryGenerator_ProjectedPoints::ProjectAndFilterNavPoints(TArray<FVector>& Points, const ANavigationData* NavData) const
{
	const FVector VerticalOffset(0, 0, ProjectionData.PostProjectionVerticalOffset);

	if (ProjectionData.TraceMode == EEnvQueryTrace::Navigation && NavData)
	{
		TSharedPtr<const FNavigationQueryFilter> NavigationFilter = UNavigationQueryFilter::GetQueryFilter(NavData, ProjectionData.NavigationFilter);	
		const ARecastNavMesh* NavMesh = Cast<const ARecastNavMesh>(NavData);
		TArray<FNavigationProjectionWork> Workload;
		Workload.Reserve(Points.Num());

		if (ProjectionData.ProjectDown == ProjectionData.ProjectUp)
		{
			for (const auto& Point : Points)
			{
				Workload.Add(FNavigationProjectionWork(Point));
			}
		}
		else
		{
			const FVector VerticalOffset = FVector(0, 0, (ProjectionData.ProjectUp - ProjectionData.ProjectDown) / 2);
			for (const auto& Point : Points)
			{
				Workload.Add(FNavigationProjectionWork(Point + VerticalOffset));
			}
		}

		const FVector ProjectionExtent(ProjectionData.ExtentX, ProjectionData.ExtentX, (ProjectionData.ProjectDown + ProjectionData.ProjectUp) / 2);

		NavData->BatchProjectPoints(Workload, ProjectionExtent, NavigationFilter);

		for (int32 PointIndex = Workload.Num() - 1; PointIndex >= 0; PointIndex--)
		{
			if (Workload[PointIndex].bResult == false)
			{
				Points.RemoveAt(PointIndex);
			}
			else
			{
				Points[PointIndex] = Workload[PointIndex].OutLocation.Location + VerticalOffset;
			}
		}
	}
}
