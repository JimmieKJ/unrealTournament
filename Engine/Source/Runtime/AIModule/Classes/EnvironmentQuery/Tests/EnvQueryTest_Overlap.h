// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvQueryTest_Overlap.generated.h"

class UEnvQueryContext;

UCLASS(MinimalAPI)
class UEnvQueryTest_Overlap : public UEnvQueryTest
{
	GENERATED_UCLASS_BODY()

	/** Overlap data */
	UPROPERTY(EditDefaultsOnly, Category=Overlap)
	FEnvOverlapData OverlapData;

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:

	DECLARE_DELEGATE_RetVal_SixParams(bool, FRunOverlapSignature, const FVector&, const FCollisionShape&, AActor*, UWorld*, enum ECollisionChannel, const FCollisionQueryParams&);

	bool RunOverlap(const FVector& ItemPos, const FCollisionShape& CollisionShape, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params);
	bool RunOverlapBlocking(const FVector& ItemPos, const FCollisionShape& CollisionShape, AActor* ItemActor, UWorld* World, enum ECollisionChannel Channel, const FCollisionQueryParams& Params);
};
