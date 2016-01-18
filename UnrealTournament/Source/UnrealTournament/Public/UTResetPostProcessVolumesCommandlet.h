// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Commandlets/Commandlet.h"
#include "UTResetPostProcessVolumesCommandlet.generated.h"

UCLASS()
class UUTResetPostProcessVolumesCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface
};