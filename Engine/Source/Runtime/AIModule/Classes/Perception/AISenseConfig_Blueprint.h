// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AISenseConfig.h"
#include "AISenseConfig_Blueprint.generated.h"

class UAISense_Blueprint;

UCLASS(Blueprintable, Abstract)
class AIMODULE_API UAISenseConfig_Blueprint : public UAISenseConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", NoClear, config)
	TSubclassOf<UAISense_Blueprint> Implementation;

	virtual TSubclassOf<UAISense> GetSenseImplementation() const override { return Implementation; }
};
