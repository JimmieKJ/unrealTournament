// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlayerController.h"

#include "UTDemoRecSpectator.generated.h"

UCLASS()
class AUTDemoRecSpectator : public AUTPlayerController
{
	GENERATED_UCLASS_BODY()

	virtual bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;
};