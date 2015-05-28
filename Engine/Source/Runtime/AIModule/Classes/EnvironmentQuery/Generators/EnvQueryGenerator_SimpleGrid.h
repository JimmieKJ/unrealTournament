// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"
#include "EnvQueryGenerator_SimpleGrid.generated.h"

class UEnvQueryContext;

/**
 *  Simple grid, generates points in 2D square around context
 */

UCLASS(meta = (DisplayName = "Points: Grid"))
class UEnvQueryGenerator_SimpleGrid : public UEnvQueryGenerator_ProjectedPoints
{
	GENERATED_UCLASS_BODY()

	/** square's extent */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	FAIDataProviderFloatValue GridSize;

	/** generation density */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	FAIDataProviderFloatValue SpaceBetween;

	/** context */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	TSubclassOf<UEnvQueryContext> GenerateAround;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
};
