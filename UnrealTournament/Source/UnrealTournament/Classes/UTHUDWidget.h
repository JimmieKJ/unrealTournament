// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 * This is a hud widget.  UT Hud widgets are not on the micro level.  They are not labels or images rather  
 * a collection of base elements that display some form of data from the game.  For example it might be a 
 * health indicator, or a weapon bar, or a message zone.  
 *
 * NOTE: This is completely WIP code.  It will change drasticly as we determine what we will need.
 *
 **/

#include "UTHUDWidget.generated.h"

UCLASS()
class UUTHUDWidget : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	// This is the position of this widget defined as a percentage of screen space.  At Draw time, this will be translated in to RenderPosition
	UPROPERTY()
	FVector2D Position;


	// Hide/show this widget
	virtual bool IsHidden();
	virtual void SetHidden(bool bIsHidden);

	virtual void PreDraw(AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter);
	virtual void Draw(float DeltaTime);
	virtual void PostDraw();
protected:

	// The UTHUD that owns this widget.  
	UPROPERTY()
	class AUTHUD* UTHUDOwner;

	// if TRUE, this widget will not be rendered
	UPROPERTY()
	uint32 bHidden:1;

	// The last time this widget was rendered
	UPROPERTY()
	float LastRenderTime;

	// RenderPosition is only valid during the rendering potion and hols the postion in screenspace at the current resolution.
	UPROPERTY()
	FVector2D RenderPosition;

	UPROPERTY()
	FVector2D CanvasCenter;

	// Draw text on the screen with supplied justification
	void DrawText(FText Text, float X, float Y, UFont* Font, FLinearColor Color, ETextHorzPos::Type TextHorzPosition = ETextHorzPos::Left, ETextVertPos::Type TextVertPosition = ETextVertPos::Top);

private:
	class UCanvas* Canvas;

};
