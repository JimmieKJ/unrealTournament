// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTHUD_CTF.h"
#include "SUTHudWindow.h"

#include "UTFlagRunHUD.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTFlagRunHUD : public AUTHUD_CTF
{
	GENERATED_UCLASS_BODY()

	virtual void DrawHUD() override;

	/** icon for player starts on the minimap (foreground) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	FCanvasIcon PlayerStartIcon;

	int32 RedPlayerCount;
	int32 BluePlayerCount;
	float RedDeathTime;
	float BlueDeathTime;

	virtual EInputMode::Type GetInputMode_Implementation() const;

protected:
	bool IsTeamOnOffense(AUTPlayerState* PS) const;

	bool bConstructedPowerupWindowForDefense;
	TSharedPtr<SUTHUDWindow> PowerupSelectWindow;
};