// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Commandlets/Commandlet.h"

#include "GenerateLoadOrderFileCommandlet.generated.h"

UCLASS()
class UGenerateLoadOrderFileCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface
};

