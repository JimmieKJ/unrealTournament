// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AISenseConfig.h"
#include "AISenseConfig_Prediction.generated.h"

class UAISense_Prediction;

UCLASS(meta = (DisplayName = "AI Prediction sense config"))
class AIMODULE_API UAISenseConfig_Prediction : public UAISenseConfig
{
	GENERATED_UCLASS_BODY()
public:	
	virtual TSubclassOf<UAISense> GetSenseImplementation() const override;
};