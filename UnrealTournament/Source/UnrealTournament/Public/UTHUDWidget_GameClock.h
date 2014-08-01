// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_GameClock.generated.h"

UCLASS()
class UUTHUDWidget_GameClock : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UTexture* HudTexture;

	virtual void Draw_Implementation(float DeltaTime);

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return true;
	}


};