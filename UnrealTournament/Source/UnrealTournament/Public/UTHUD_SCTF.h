// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTFlagRunHUD.h"
#include "UTHUD_SCTF.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTHUD_SCTF : public AUTFlagRunHUD
{
	GENERATED_UCLASS_BODY()
	virtual void DrawMinimapSpectatorIcons() override;
	void DrawHUD();

protected:
	virtual void HandlePowerups();

};