// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvQueryGenerator_ActorsOfClass.generated.h"

class AActor;
class UEnvQueryContext;

UCLASS(meta = (DisplayName = "Actors Of Class"))
class UEnvQueryGenerator_ActorsOfClass : public UEnvQueryGenerator
{
	GENERATED_UCLASS_BODY()

	/** max distance of path between point and context */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	FAIDataProviderFloatValue SearchRadius;

	UPROPERTY(EditDefaultsOnly, Category=Generator)
	TSubclassOf<AActor> SearchedActorClass;

	/** context */
	UPROPERTY(EditAnywhere, Category=Generator)
	TSubclassOf<UEnvQueryContext> SearchCenter;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
};
