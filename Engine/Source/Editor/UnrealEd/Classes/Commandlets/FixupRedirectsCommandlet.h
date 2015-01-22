// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Commandlets/Commandlet.h"
#include "FixupRedirectsCommandlet.generated.h"

UCLASS()
class UFixupRedirectsCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()
	// Begin UCommandlet Interface
	virtual void CreateCustomEngine(const FString& Params) override;
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface
};


