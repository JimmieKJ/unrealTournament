// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/GatherTextCommandletBase.h"
#include "RepairLocalizationDataCommandlet.generated.h"


UCLASS()
class URepairLocalizationDataCommandlet : public UGatherTextCommandletBase
{
	GENERATED_UCLASS_BODY()

public:
	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface
};