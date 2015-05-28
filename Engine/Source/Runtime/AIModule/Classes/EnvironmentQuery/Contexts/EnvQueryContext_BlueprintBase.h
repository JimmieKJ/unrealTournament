// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_BlueprintBase.generated.h"

class UWorld;
struct FEnvQueryInstance;
struct FEnvQueryContextData;

UCLASS(MinimalAPI, Abstract, Blueprintable)
class UEnvQueryContext_BlueprintBase : public UEnvQueryContext
{
	GENERATED_UCLASS_BODY()

	enum ECallMode
	{
		InvalidCallMode,
		SingleActor,
		SingleLocation,
		ActorSet,
		LocationSet
	};

	ECallMode CallMode;

	// We need to implement GetWorld() so that blueprint functions which use a hidden WorldContextObject* will work properly.
	virtual UWorld* GetWorld() const override;

	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;

	UFUNCTION(BlueprintImplementableEvent)
	void ProvideSingleActor(AActor* QuerierActor, AActor*& ResultingActor) const;

	UFUNCTION(BlueprintImplementableEvent)
	void ProvideSingleLocation(AActor* QuerierActor, FVector& ResultingLocation) const;

	UFUNCTION(BlueprintImplementableEvent)
	void ProvideActorsSet(AActor* QuerierActor, TArray<AActor*>& ResultingActorsSet) const;

	UFUNCTION(BlueprintImplementableEvent)
	void ProvideLocationsSet(AActor* QuerierActor, TArray<FVector>& ResultingLocationSet) const;
};
