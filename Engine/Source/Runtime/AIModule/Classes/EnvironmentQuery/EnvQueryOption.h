// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvQueryOption.generated.h"

class UEnvQueryGenerator;
class UEnvQueryTest;

UCLASS()
class AIMODULE_API UEnvQueryOption : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UEnvQueryGenerator* Generator;

	UPROPERTY()
	TArray<UEnvQueryTest*> Tests;

	FText GetDescriptionTitle() const;
	FText GetDescriptionDetails() const;
};
