// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"
#include "EnvironmentQuery/EnvQueryTraceHelpers.h"

UEnvQueryGenerator_ProjectedPoints::UEnvQueryGenerator_ProjectedPoints(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ProjectionData.bCanProjectDown = true;
	ProjectionData.bCanDisableTrace = true;
	ProjectionData.ExtentX = 0.0f;

	ItemType = UEnvQueryItemType_Point::StaticClass();
}

void UEnvQueryGenerator_ProjectedPoints::ProjectAndFilterNavPoints(TArray<FNavLocation>& Points, FEnvQueryInstance& QueryInstance) const
{
	const ANavigationData* NavData = nullptr;
	if (ProjectionData.TraceMode != EEnvQueryTrace::None)
	{
		NavData = FEQSHelpers::FindNavigationDataForQuery(QueryInstance);
	}

	if (NavData)
	{
		FEQSHelpers::ETraceMode Mode = (ProjectionData.TraceMode == EEnvQueryTrace::Navigation) ? FEQSHelpers::ETraceMode::Discard : FEQSHelpers::ETraceMode::Keep;
		FEQSHelpers::RunNavProjection(*NavData, ProjectionData, Points, Mode);
	}

	if (ProjectionData.TraceMode == EEnvQueryTrace::Geometry)
	{
		FEQSHelpers::RunPhysProjection(QueryInstance.World, ProjectionData, Points);
	}
}

void UEnvQueryGenerator_ProjectedPoints::StoreNavPoints(const TArray<FNavLocation>& Points, FEnvQueryInstance& QueryInstance) const
{
	const int32 InitialElementsCount = QueryInstance.Items.Num();
	QueryInstance.ReserveItemData(InitialElementsCount + Points.Num());
	for (int32 Idx = 0; Idx < Points.Num(); Idx++)
	{
		// store using default function to handle creating new data entry 
		QueryInstance.AddItemData<UEnvQueryItemType_Point>(Points[Idx]);
	}

	FEnvQueryOptionInstance& OptionInstance = QueryInstance.Options[QueryInstance.OptionIndex];
	OptionInstance.bHasNavLocations = (ProjectionData.TraceMode == EEnvQueryTrace::Navigation);
}
