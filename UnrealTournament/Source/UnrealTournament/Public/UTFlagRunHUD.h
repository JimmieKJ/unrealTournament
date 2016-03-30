// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTHUD_CTF.h"
#include "UTFlagRunHUD.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTFlagRunHUD : public AUTHUD_CTF
{
	GENERATED_UCLASS_BODY()

	virtual void AUTFlagRunHUD::DrawHUD() override;

	/** icon for player starts on the minimap (foreground) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
		FCanvasIcon PlayerStartIcon;

	int32 RedPlayerCount;
	int32 BluePlayerCount;
	int32 RedDeathCount;
	int32 BlueDeathCount;
	float RedDeathTime;
	float BlueDeathTime;
};