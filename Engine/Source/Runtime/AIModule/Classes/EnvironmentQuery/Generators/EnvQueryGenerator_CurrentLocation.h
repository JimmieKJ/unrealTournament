// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvQueryGenerator_CurrentLocation.generated.h"

struct FEnvQueryInstance;

UCLASS(meta = (DisplayName = "Current Location"))
class AIMODULE_API UEnvQueryGenerator_CurrentLocation : public UEnvQueryGenerator
{
	GENERATED_BODY()

protected:
	/** context */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	TSubclassOf<UEnvQueryContext> QueryContext;

public:

	UEnvQueryGenerator_CurrentLocation(const FObjectInitializer& ObjectInitializer);
	
	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
};
