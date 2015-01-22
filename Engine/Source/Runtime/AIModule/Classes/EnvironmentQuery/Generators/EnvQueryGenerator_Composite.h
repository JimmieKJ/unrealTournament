// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvQueryGenerator_Composite.generated.h"

class UEnvQueryGenerator;
struct FEnvQueryInstance;

/**
 * Composite generator allows using multiple generators in single query option
 * Currently it's available only from code
 */

UCLASS(MinimalAPI)
class UEnvQueryGenerator_Composite : public UEnvQueryGenerator
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<UEnvQueryGenerator*> Generators;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
};
