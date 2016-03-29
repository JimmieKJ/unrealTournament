// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_Project.generated.h"

struct FEnvQueryInstance;

/**
 * Projects points on navigation or geometry, will modify value of projected items.
 * Works only on item type: point
 */
UCLASS(MinimalAPI)
class UEnvQueryTest_Project : public UEnvQueryTest
{
	GENERATED_UCLASS_BODY()

protected:

	/** trace params */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	FEnvTraceData ProjectionData;

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionDetails() const override;
};
