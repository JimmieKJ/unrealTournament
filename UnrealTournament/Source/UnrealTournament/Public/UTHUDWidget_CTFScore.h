// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_TeamScore.h"
#include "UTCTFFlagBase.h"
#include "UTHUDWidget_CTFScore.generated.h"


UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_CTFScore : public UUTHUDWidget_TeamScore
{
	GENERATED_UCLASS_BODY()

	float RedPulseScale;
	float BluePulseScale;

	virtual void Draw_Implementation(float DeltaTime);
	virtual void DrawBasePosition(AUTCTFFlagBase* Base, float CenterX, float CenterY, float Scale);
	virtual void DrawFlagIcon(float CenterX, float CenterY, float Width, float Height, float U, float V, float UL, float VL, FLinearColor DrawColor, float Scale);
};