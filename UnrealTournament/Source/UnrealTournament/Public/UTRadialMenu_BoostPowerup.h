// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget.h"
#include "UTRadialMenu.h"
#include "UTRadialMenu_BoostPowerup.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTRadialMenu_BoostPowerup : public UUTRadialMenu
{
	GENERATED_UCLASS_BODY()

public:
	virtual void InitializeWidget(AUTHUD* Hud);
	virtual void BecomeInteractive() override
	{
		Super::BecomeInteractive();
		CurrentSegment = INDEX_NONE;
	}

protected:
	virtual void Execute();
	virtual void DrawMenu(FVector2D ScreenCenter, float RenderDelta);
	
	virtual bool ShouldDraw_Implementation(bool bShowScores) override;
};

