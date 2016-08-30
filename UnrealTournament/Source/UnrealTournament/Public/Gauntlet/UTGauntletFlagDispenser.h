// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTGauntletFlag.h"
#include "UTCTFFlagBase.h"
#include "UTGauntletFlagDispenser.generated.h"

UCLASS(HideCategories = GameObject)
class UNREALTOURNAMENT_API AUTGauntletFlagDispenser : public AUTCTFFlagBase
{
	GENERATED_UCLASS_BODY()

	// Called to create the necessary capture objects.
	virtual void CreateFlag();

	// Called after each round
	virtual void Reset() override;

	virtual void PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir);
	virtual FText GetHUDStatusMessage(AUTHUD* HUD);

protected:
	// We override CreateCarriedObject as it's no longer used.  CreateGauntletObject() will be called by the game mode
	// at the appropriate time instead.
	virtual void CreateCarriedObject();


};