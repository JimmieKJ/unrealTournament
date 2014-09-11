// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_TeamScore.h"
#include "UTHUDWidget_CTFScore.generated.h"

UCLASS()
class UUTHUDWidget_CTFScore : public UUTHUDWidget_TeamScore
{
	GENERATED_UCLASS_BODY()

	float RedPulseScale;
	float BluePulseScale;

	virtual void Draw_Implementation(float DeltaTime);
	virtual void DrawFlagIcon(float CenterX, float CenterY, float Width, float Height, float U, float V, float UL, float VL, FLinearColor DrawColor, float Scale);
};