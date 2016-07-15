// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_Boost.generated.h"

const float UNLOCK_ANIM_DURATION =0.65f;

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_Boost : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void InitializeWidget(AUTHUD* Hud);
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores);
	
private:
	UPROPERTY()
	UMaterialInterface* HudTimerMI;

	UPROPERTY()
	UMaterialInstanceDynamic* HudTimerMID;
	float IconScale;

	bool bLastUnlocked;
	float UnlockAnimTime;


};
