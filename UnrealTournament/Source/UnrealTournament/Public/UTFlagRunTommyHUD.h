// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTFlagRunHUD.h"
#include "UTFlagRunTommyHUD.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTFlagRunTommyHUD : public AUTFlagRunHUD
{
	GENERATED_UCLASS_BODY()

	virtual void DrawHUD() override;

protected:
	bool IsTeamOnOffense(AUTPlayerState* PS) const;
};