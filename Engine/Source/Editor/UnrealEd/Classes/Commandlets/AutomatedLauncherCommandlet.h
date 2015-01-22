// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/** Commandlet for running automated commands through the launcher */

#pragma once
#include "Commandlets/Commandlet.h"
#include "AutomatedLauncherCommandlet.generated.h"

UCLASS()
class UAutomatedLauncherCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface
};