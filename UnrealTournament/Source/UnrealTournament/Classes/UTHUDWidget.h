// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 * This is a hud widget.  UT Hud widgets are not on the micro level.  They are not labels or images rather  
 * a collection of base elements that display some form of data from the game.  For example it might be a 
 * health indicator, or a weapon bar, or a message zone.  
 *
 * NOTE: This is completely WIP code.  It will change drastically as we determine what we will need.
 *
 **/

#include "UTHUDWidget.generated.h"

const float WIDGET_DEFAULT_Y_RESOLUTION = 1080;	// designing for 1080p (19200x1080).  Widgets will be reposition/size against this number.

UCLASS(BlueprintType, Blueprintable)
class UUTHUDWidget : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	// This is the position of the widget relative to the Screen position and origin.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widgets")
	FVector2D Position;

	// The size of the widget as designed.  NOTE: Currently no clipping occurs so it's very possible
	// to blow out beyond the bounds.  
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widgets")
	FVector2D Size;

	// The widget's position is relative to this origin.  Normalized.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widgets")
	FVector2D Origin;

	// Holds Widget's normalized position relative to the  actual display.  Useful for quickly snapping
	// the widget to the right or bottom edge and can be used with Negative position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widgets")
	FVector2D ScreenPosition;

	// If true, then this widget will attempt to scale itself by to the designed resolution.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widgets")
	uint32 bScaleByDesignedResolution:1;

	// If true, any scaling will maintain the aspect ratio of the widget.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widgets")
	uint32 bMaintainAspectRatio:1;

	// Hide/show this widget.
	virtual bool IsHidden();

	// Sets the visibility of this widget
	UFUNCTION(BlueprintCallable, Category="Widgets")
	virtual void SetHidden(bool bIsHidden);

	// Predraw is called before the main drawing function in order to perform any needed scaling / positioning /etc as
	// well as cache the canvas and owner.
	virtual void PreDraw(AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter);

	// The main drawing function
	virtual void Draw(float DeltaTime);

	// This function can be used to override the regular draw system of a HudWidgets.  Return TRUE if you want to
	// completely short circuit drawing after the Blueprint implemented draw is finished.
	UFUNCTION(BlueprintImplementableEvent)
	bool eventDraw(float DeltaTime);

	virtual void PostDraw(float RenderedTime);



protected:

	// The UTHUD that owns this widget.  
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	class AUTHUD* UTHUDOwner;

	// if TRUE, this widget will not be rendered
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	uint32 bHidden:1;

	// The last time this widget was rendered
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	float LastRenderTime;

	// RenderPosition is only valid during the rendering potion and holds the position in screen space at the current resolution.
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	FVector2D RenderPosition;
	
	// Scaled sizing.  NOTE: respects bMaintainAspectRatio
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	FVector2D RenderSize;

	// The Scale based on the designed resolution
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	float RenderScale;

	// The precalculated center of the canvas
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	FVector2D CanvasCenter;

	// The cached reference to the UCanvas object
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	class UCanvas* Canvas;

	/**
	 * Draws text on the screen.  You can use the TextHorzPosition and TextVertPosition to justify the text that you are drawing.
	 * @param Text	The Text to display.
	 * @param X		The X position relative to the widget
	 * @param Y		The Y position relative to the widget
	 * @param Font	The font to use for the text
	 * @param TextHorzAlignment	How to align the text horizontally within the widget
	 * @param TextVertAlignment How to align the text vertically within the widget
	 **/
	UFUNCTION(BlueprintCallable, Category="Widgets")
	virtual void DrawText(FText Text, float X, float Y, UFont* Font, FLinearColor DrawColor, ETextHorzPos::Type TextHorzAlignment = ETextHorzPos::Left, ETextVertPos::Type TextVertAlignment = ETextVertPos::Top);
	
	/**
	 * Draws a texture on the screen.
	 * @param Texture		The texture to draw on the screen
	 * @param X				The X position relative to the widget
	 * @param Y				The Y position relative to the widget
	 * @param Width			The Width (in pixels) of the image to display.  If the widget is scaled, this value will be as well.
	 * @param Height		The Height (in pixels) of the image to display.  If the widget is scaled, this value will be as well.
	 * @param MaterialU		The normalize left position within the source texture
	 * @param MaterialV		The normalize top position within the source texture
	 * @param MaterialUL	The normalize right position within the source texture
	 * @param MaterialVL	The normalize bottom position within the source texture
	 * @param DrawColor		The Color to use when drawing this texture
	 * @param RenderOffset	When drawing this image, these offsets will be applied to the position.  They are normalized to the width and height of the image being draw.
	 * @param Rotation		The widget will be rotated by this value.  In Degrees
	 * @param RotPivot		The point at which within the image that the rotation will be around
	 **/
	UFUNCTION(BlueprintCallable, Category="Widgets")
	virtual void DrawTexture(UTexture* Texture, float X, float Y, float Width, float Height, float MaterialU=0.0f, float MaterialV=0.0f, float MaterialUL=1.0, float MaterialVL=1.0f, FLinearColor DrawColor=FLinearColor::White, FVector2D RenderOffset=FVector2D(0.0f, 0.0f), float Rotation=0, FVector2D RotPivot=FVector2D(0.5f, 0.5f));

	/**
	 * Draws a material on the screen.
	 * @param Texture		The material to draw on the screen
	 * @param X				The X position relative to the widget
	 * @param Y				The Y position relative to the widget
	 * @param Width			The Width (in pixels) of the image to display.  If the widget is scaled, this value will be as well.
	 * @param Height		The Height (in pixels) of the image to display.  If the widget is scaled, this value will be as well.
	 * @param MaterialU		The normalize left position within the source texture
	 * @param MaterialV		The normalize top position within the source texture
	 * @param MaterialUL	The normalize right position within the source texture
	 * @param MaterialVL	The normalize bottom position within the source texture
	 * @param DrawColor		The Color to use when drawing this texture
	 * @param RenderOffset	When drawing this image, these offsets will be applied to the position.  They are normalized to the width and height of the image being draw.
	 * @param Rotation		The widget will be rotated by this value.  In Degrees
	 * @param RotPivot		The point at which within the image that the rotation will be around
	 **/
	UFUNCTION(BlueprintCallable, Category="Widgets")
	virtual void DrawMaterial( UMaterialInterface* Material, float X, float Y, float Width, float Height, float MaterialU, float MaterialV, float MaterialUWidth, float MaterialVHeight, FVector2D RenderOffset=FVector2D(0.0f, 0.0f), float Rotation=0, FVector2D RotPivot=FVector2D(0.5f, 0.5f));
};
