// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_SimpleGrid.h"
#include "AI/Navigation/RecastNavMesh.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryGenerator_SimpleGrid::UEnvQueryGenerator_SimpleGrid(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	GenerateAround = UEnvQueryContext_Querier::StaticClass();
	ItemType = UEnvQueryItemType_Point::StaticClass();
	GridSize.DefaultValue = 500.0f;
	SpaceBetween.DefaultValue = 100.0f;

	// keep deprecated properties initialized
	Radius.Value = 500.0f;
	Density.Value = 500.0f;
}

void UEnvQueryGenerator_SimpleGrid::PostLoad()
{
	if (VerNum < EnvQueryGeneratorVersion::DataProviders)
	{
		Radius.Convert(this, GridSize);
		Density.Convert(this, SpaceBetween);
	}

	Super::PostLoad();
}

void UEnvQueryGenerator_SimpleGrid::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	UObject* BindOwner = QueryInstance.Owner.Get();
	GridSize.BindData(BindOwner, QueryInstance.QueryID);
	SpaceBetween.BindData(BindOwner, QueryInstance.QueryID);

	float RadiusValue = GridSize.GetValue();
	float DensityValue = SpaceBetween.GetValue();

	const bool bProjectToNavigation = (ProjectionData.TraceMode == EEnvQueryTrace::Navigation);
#if WITH_RECAST
	const ARecastNavMesh* NavMesh = bProjectToNavigation ? FEQSHelpers::FindNavMeshForQuery(QueryInstance) : NULL;
	if (bProjectToNavigation && NavMesh == NULL) 
	{
		return;
	}
#endif // WITH_RECAST

	const int32 ItemCount = FPlatformMath::TruncToInt((RadiusValue * 2.0f / DensityValue) + 1);
	const int32 ItemCountHalf = ItemCount / 2;

	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(GenerateAround, ContextLocations);

#if WITH_RECAST
	const FVector ProjectExtent(ProjectionData.ExtentX, ProjectionData.ExtentX, 0);
	if (NavMesh)
	{
		NavMesh->BeginBatchQuery();
	}
#endif // WITH_RECAST

	TArray<FVector> GridPoints;
	GridPoints.Reserve(ItemCount * ItemCount * ContextLocations.Num());

	for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ContextIndex++)
	{
		for (int32 IndexX = 0; IndexX <= ItemCount; ++IndexX)
		{
			for (int32 IndexY = 0; IndexY <= ItemCount; ++IndexY)
			{
				const FVector TestPoint = ContextLocations[ContextIndex] - FVector(DensityValue * (IndexX - ItemCountHalf), DensityValue * (IndexY - ItemCountHalf), 0);
				GridPoints.Add(TestPoint);
			}
		}
	}

#if WITH_RECAST
	if (NavMesh)
	{
		ProjectAndFilterNavPoints(GridPoints, NavMesh);
		NavMesh->FinishBatchQuery();
	}
#endif // WITH_RECAST

	QueryInstance.ReserveItemData(GridPoints.Num());
	for (int32 PointIndex = 0; PointIndex < GridPoints.Num(); PointIndex++)
	{
		QueryInstance.AddItemData<UEnvQueryItemType_Point>(GridPoints[PointIndex]);
	}
}

FText UEnvQueryGenerator_SimpleGrid::GetDescriptionTitle() const
{
	return FText::Format(LOCTEXT("DescriptionGenerateAroundContext", "{0}: generate around {1}"),
		Super::GetDescriptionTitle(), UEnvQueryTypes::DescribeContext(GenerateAround));
};

FText UEnvQueryGenerator_SimpleGrid::GetDescriptionDetails() const
{
	FText Desc = FText::Format(LOCTEXT("SimpleGridDescription", "size: {0}, space between: {1}"),
		FText::FromString(GridSize.ToString()), FText::FromString(SpaceBetween.ToString()));

	FText ProjDesc = ProjectionData.ToText(FEnvTraceData::Brief);
	if (!ProjDesc.IsEmpty())
	{
		FFormatNamedArguments ProjArgs;
		ProjArgs.Add(TEXT("Description"), Desc);
		ProjArgs.Add(TEXT("ProjectionDescription"), ProjDesc);
		Desc = FText::Format(LOCTEXT("SimpleGridDescriptionWithProjection", "{Description}, {ProjectionDescription}"), ProjArgs);
	}

	return Desc;
}

#undef LOCTEXT_NAMESPACE