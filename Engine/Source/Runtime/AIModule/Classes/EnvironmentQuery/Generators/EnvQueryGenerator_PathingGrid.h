// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/Generators/EnvQueryGenerator_SimpleGrid.h"
#include "EnvQueryGenerator_PathingGrid.generated.h"

class UNavigationQueryFilter;

/**
 *  Navigation grid, generates points on navmesh
 *  with paths to/from context no further than given limit
 */

UCLASS(meta = (DisplayName = "Points: Pathing Grid"))
class UEnvQueryGenerator_PathingGrid : public UEnvQueryGenerator_SimpleGrid
{
	GENERATED_UCLASS_BODY()

	/** pathfinding direction */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderBoolValue PathToItem;

	/** navigation filter to use in pathfinding */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	TSubclassOf<UNavigationQueryFilter> NavigationFilter;

	/** multiplier for max distance between point and context */
	UPROPERTY(EditDefaultsOnly, Category = Pathfinding, AdvancedDisplay)
	FAIDataProviderFloatValue ScanRangeMultiplier;

	virtual void ProjectAndFilterNavPoints(TArray<FNavLocation>& Points, FEnvQueryInstance& QueryInstance) const override;
};
