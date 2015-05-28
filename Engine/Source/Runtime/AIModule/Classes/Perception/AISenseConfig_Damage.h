// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AISenseConfig.h"
#include "AISense_Damage.h"
#include "AISenseConfig_Damage.generated.h"

class UAISense_Damage;

UCLASS(meta = (DisplayName = "AI Damage sense config"))
class AIMODULE_API UAISenseConfig_Damage : public UAISenseConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", NoClear, config)
	TSubclassOf<UAISense_Damage> Implementation;

	virtual TSubclassOf<UAISense> GetSenseImplementation() const override;
};
