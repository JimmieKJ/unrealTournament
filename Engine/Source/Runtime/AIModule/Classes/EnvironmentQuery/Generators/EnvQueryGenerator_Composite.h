// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvQueryGenerator_Composite.generated.h"

/**
 * Composite generator allows using multiple generators in single query option
 * All child generators must produce exactly the same item type!
 */

UCLASS(meta = (DisplayName = "Composite"))
class AIMODULE_API UEnvQueryGenerator_Composite : public UEnvQueryGenerator
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, Instanced, Category = Generator)
	TArray<UEnvQueryGenerator*> Generators;

	UPROPERTY()
	bool bHasMatchingItemType;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;

	void VerifyItemTypes();
};
