// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLoadMap.h"

#include "UTGameEngine.generated.h"

UCLASS()
class UUTGameEngine : public UGameEngine
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FText ReadEULACaption;
	UPROPERTY()
	FText ReadEULAText;

	/** used to display EULA info on first run */
	UPROPERTY(globalconfig)
	bool bFirstRun;

	virtual void Init(IEngineLoop* InEngineLoop);

	UT_LOADMAP_DEFINITION()
};