// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_ReplayTimeSlider.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_ReplayTimeSlider : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	FVector2D MousePosition;
	bool bIsInteractive;
	int32 CachedSliderLength;
	int32 CachedSliderOffset;
	int32 CachedSliderHeight;
	int32 CachedSliderYPos;

public:
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores);
	virtual void InitializeWidget(AUTHUD* Hud);

	virtual void TrackMouseMovement(FVector2D InMousePosition) { MousePosition = InMousePosition; }
	virtual bool SelectionClick(FVector2D InMousePosition);
	void BecomeInteractive() { bIsInteractive = true; }
	void BecomeNonInteractive() { bIsInteractive = false; }
};