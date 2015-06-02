// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_ReplayTimeSlider.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_ReplayTimeSlider : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores);
	virtual void InitializeWidget(AUTHUD* Hud);
};