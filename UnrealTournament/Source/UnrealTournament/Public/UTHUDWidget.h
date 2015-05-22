// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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

#include "UTATypes.h"
#include "CanvasTypes.h"
#include "UTHUDWidget.generated.h"

const float WIDGET_DEFAULT_Y_RESOLUTION = 1080;	// We design everything against 1080p

/** UT version of FCanvasTextItem that properly handles distance field font features */
struct FUTCanvasTextItem : public FCanvasTextItem
{
	FUTCanvasTextItem(const FVector2D& InPosition, const FText& InText, class UFont* InFont, const FLinearColor& InColor, const TSharedPtr<FCanvasWordWrapper> InWordWrapper = NULL)
	: FCanvasTextItem(InPosition, InText, InFont, InColor), CharIncrement(0.0f), WrapXL(0.0f), WordWrapper(InWordWrapper)
	{}

	virtual void Draw(class FCanvas* InCanvas) override;

	float CharIncrement;
	// word wrap size, if render info has !bClipText
	float WrapXL;
	
	TSharedPtr<FCanvasWordWrapper> WordWrapper;

protected:
	// UT version appropriately handles distance field fonts by slightly overlapping triangles to give the shadows more space
	void UTDrawStringInternal(class FCanvas* InCanvas, const FVector2D& DrawPos, const FLinearColor& DrawColor);
private:
	void DrawStringInternal(class FCanvas* InCanvas, const FVector2D& DrawPos, const FLinearColor& DrawColor)
	{
		UTDrawStringInternal(InCanvas, DrawPos, DrawColor);
	}

	void DrawStringInternal_HackyFix( FCanvas* InCanvas, const FVector2D& DrawPos, const FLinearColor& DrawColor );

};

namespace ERenderObjectType
{
	extern const FName TextureObject;		
	extern const FName TextObject;
}


// Forward declare
class AUTHUD;

UCLASS(BlueprintType, Blueprintable)
class UNREALTOURNAMENT_API UUTHUDWidget : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	virtual UWorld* GetWorld() const;

	// This is the position of the widget relative to the Screen position and origin.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widgets")
	float DesignedResolution;

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

	// The opacity of this widget
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widgets")
	float Opacity;

	// Whether to colorize this widget using base HUD color (like team color)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widgets")
	uint32 bIgnoreHUDBaseColor:1;
	
	// Will be called as soon as the widget is created and tracked by the hud.
	virtual void InitializeWidget(AUTHUD* Hud);

	// Hide/show this widget.
	virtual bool IsHidden();

	// Sets the visibility of this widget
	UFUNCTION(BlueprintCallable, Category="Widgets")
	virtual void SetHidden(bool bIsHidden);

	// @Returns true if this widget should be drawn.
	UFUNCTION(BlueprintNativeEvent)
	bool ShouldDraw(bool bShowScores);

	// Predraw is called before the main drawing function in order to perform any needed scaling / positioning /etc as
	// well as cache the canvas and owner.  
	virtual void PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter);

	// The main drawing function.  When it's called, UTHUDOwner, UTPlayerOwner, UTCharacter and Canvas will all
	// be valid.  If you are override this event in Blueprint, you should make sure you 
	// understand the ramifications of calling or not calling the Parent/Super version of this function.
	UFUNCTION(BlueprintNativeEvent)
	void Draw(float DeltaTime);

	virtual void PostDraw(float RenderedTime);

	// The UTHUD that owns this widget.  
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	class AUTHUD* UTHUDOwner;

	// Cached reference to the player controller that "owns" this Widget.  ONLY VALID During the render phase
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	class AUTPlayerController* UTPlayerOwner;

	// Cached reference to UTCharacter (if any) that "owns" this widget.  ONLY VALID during the render phase
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	class AUTCharacter* UTCharacterOwner;

	// Cached reference to the UTGameState for the current match.  ONLY VALID during the render phase
	UPROPERTY(BlueprintReadOnly, Category = "Widgets Live")
	class AUTGameState* UTGameState;

	virtual UCanvas* GetCanvas();
	virtual FVector2D GetRenderPosition();
	virtual FVector2D GetRenderSize();
	virtual float GetRenderScale();

	// Sets the visibility of this widget
	UFUNCTION(BlueprintCallable, Category="Widgets")
	virtual FLinearColor ApplyHUDColor(FLinearColor DrawColor);

	bool bShowBounds;

	/** Returns clamped name for player, and caches it in the PlayerState. */
	virtual FString GetClampedName(AUTPlayerState* PS, UFont* NameFont, float NameScale, float MaxWidth);

protected:
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

	// The cached reference to the UCanvas object.  ONLY VALID during the render phase
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	class UCanvas* Canvas;

	// The scale needed to maintain aspect ratio
	UPROPERTY(BlueprintReadOnly, Category="Widgets Live")
	float AspectScale;

	TSharedPtr<FCanvasWordWrapper> WordWrapper;

public:
	/**
	 * Draws text on the screen.  You can use the TextHorzPosition and TextVertPosition to justify the text that you are drawing.
	 * @param Text	The Text to display.
	 * @param X		The X position relative to the widget
	 * @param Y		The Y position relative to the widget
	 * @param Font	The font to use for the text
	 * @param TextScale	Additional scaling to add
	 * @param DrawOpacity	The alpha to use on this draw call.  NOTE: the Widget's Opacity will be multipled in
	 * @param DrawColor	The color to draw in
	 * @param TextHorzAlignment	How to align the text horizontally within the widget
	 * @param TextVertAlignment How to align the text vertically within the widget
	 * @returns The Size (unscaled) of the text drawn
	 **/
	FVector2D DrawText(FText Text, float X, float Y, UFont* Font, float TextScale=1.0, float DrawOpacity=1.0f, FLinearColor DrawColor = FLinearColor::White, ETextHorzPos::Type TextHorzAlignment = ETextHorzPos::Left, ETextVertPos::Type TextVertAlignment = ETextVertPos::Top);

	/**
	 * Draws text on the screen.  You can use the TextHorzPosition and TextVertPosition to justify the text that you are drawing.
	 * @param Text	The Text to display.
	 * @param X		The X position relative to the widget
	 * @param Y		The Y position relative to the widget
	 * @param Font	The font to use for the text
	 * @param OutlineColor	The Color for the outline
	 * @param TextScale	Additional scaling to add
	 * @param DrawOpacity	The alpha to use on this draw call.  NOTE: the Widget's Opacity will be multipled in
	 * @param DrawColor	The color to draw in
	 * @param TextHorzAlignment	How to align the text horizontally within the widget
	 * @param TextVertAlignment How to align the text vertically within the widget
	 * @returns The Size (unscaled) of the text drawn
	 **/
	FVector2D DrawText(FText Text, float X, float Y, UFont* Font, FLinearColor OutlineColor, float TextScale=1.0, float DrawOpacity=1.0f, FLinearColor DrawColor = FLinearColor::White, ETextHorzPos::Type TextHorzAlignment = ETextHorzPos::Left, ETextVertPos::Type TextVertAlignment = ETextVertPos::Top);

	/**
	 * Draws text on the screen.  You can use the TextHorzPosition and TextVertPosition to justify the text that you are drawing.
	 * @param Text	The Text to display.
	 * @param X		The X position relative to the widget
	 * @param Y		The Y position relative to the widget
	 * @param Font	The font to use for the text
	 * @param ShadowDirection	The Direction for the shadow
	 * @param ShadowColor	The color of the shadow
	 * @param TextScale	Additional scaling to add
	 * @param DrawOpacity	The alpha to use on this draw call.  NOTE: the Widget's Opacity will be multipled in
	 * @param DrawColor	The color to draw in
	 * @param TextHorzAlignment	How to align the text horizontally within the widget
	 * @param TextVertAlignment How to align the text vertically within the widget
	 * @returns The Size (unscaled) of the text drawn
	 **/
	FVector2D DrawText(FText Text, float X, float Y, UFont* Font, FVector2D ShadowDirection = FVector2D(0,0), FLinearColor ShadowColor = FLinearColor::Black, float TextScale=1.0, float DrawOpacity=1.0f, FLinearColor DrawColor = FLinearColor::White, ETextHorzPos::Type TextHorzAlignment = ETextHorzPos::Left, ETextVertPos::Type TextVertAlignment = ETextVertPos::Top);
	
	/**
	 * Draws text on the screen.  You can use the TextHorzPosition and TextVertPosition to justify the text that you are drawing.
	 * @param Text	The Text to display.
	 * @param X		The X position relative to the widget
	 * @param Y		The Y position relative to the widget
	 * @param Font	The font to use for the text
	 * @param ShadowDirection	The Direction for the shadow
	 * @param ShadowColor	The color of the shadow
	 * @param OutlineColor	The Color for the outline
	 * @param TextScale	Additional scaling to add
	 * @param DrawOpacity	The alpha to use on this draw call.  NOTE: the Widget's Opacity will be multipled in
	 * @param DrawColor	The color to draw in
	 * @param TextHorzAlignment	How to align the text horizontally within the widget
	 * @param TextVertAlignment How to align the text vertically within the widget
	 * @returns The Size (unscaled) of the text drawn
	 **/
	FVector2D DrawText(FText Text, float X, float Y, UFont* Font, FVector2D ShadowDirection = FVector2D(0,0), FLinearColor ShadowColor = FLinearColor::Black, FLinearColor OutlineColor=FLinearColor::Black, float TextScale=1.0, float DrawOpacity=1.0f, FLinearColor DrawColor = FLinearColor::White, ETextHorzPos::Type TextHorzAlignment = ETextHorzPos::Left, ETextVertPos::Type TextVertAlignment = ETextVertPos::Top);

	/**
	* Draws text on the screen.  You can use the TextHorzPosition and TextVertPosition to justify the text that you are drawing.
	* @param Text	The Text to display.
	* @param X		The X position relative to the widget
	* @param Y		The Y position relative to the widget
	* @param Font	The font to use for the text
	* @param RenderInfo font rendering details
	* @param TextScale	Additional scaling to add
	* @param DrawOpacity	The alpha to use on this draw call.  NOTE: the Widget's Opacity will be multipled in
	* @param DrawColor	The color to draw in
	* @param TextHorzAlignment	How to align the text horizontally within the widget
	* @param TextVertAlignment How to align the text vertically within the widget
	* @returns The Size (unscaled) of the text drawn
	**/
	FVector2D DrawText(FText Text, float X, float Y, UFont* Font, const FFontRenderInfo& RenderInfo = FFontRenderInfo(), float TextScale = 1.0, float DrawOpacity = 1.0f, FLinearColor DrawColor = FLinearColor::White, ETextHorzPos::Type TextHorzAlignment = ETextHorzPos::Left, ETextVertPos::Type TextVertAlignment = ETextVertPos::Top);

	/**
	 * Draws text on the screen.  This is the Blueprint callable override.  You can use the TextHorzPosition and TextVertPosition to justify the text that you are drawing.
	 * @param Text	The Text to display.
	 * @param X		The X position relative to the widget
	 * @param Y		The Y position relative to the widget
	 * @param Font	The font to use for the text
	 * @param ShadowDirection	The Direction for the shadow
	 * @param ShadowColor	The color of the shadow
	 * @param OutlineColor	The Color for the outline
	 * @param TextScale	Additional scaling to add
	 * @param DrawOpacity	The alpha to use on this draw call.  NOTE: the Widget's Opacity will be multipled in
	 * @param DrawColor	The color to draw in
	 * @param TextHorzAlignment	How to align the text horizontally within the widget
	 * @param TextVertAlignment How to align the text vertically within the widget
	 * @returns The Size (unscaled) of the text drawn
	 **/

	UFUNCTION(BlueprintCallable, Category = "Widgets", Meta = (AutoCreateRefTerm = "RenderInfo"))
	virtual FVector2D DrawText(FText Text, float X, float Y, UFont* Font, bool bDrawShadow = false, FVector2D ShadowDirection = FVector2D(0,0), FLinearColor ShadowColor = FLinearColor::Black, bool bDrawOutline = false, FLinearColor OutlineColor = FLinearColor::Black, float TextScale = 1.0, float DrawOpacity = 1.0, FLinearColor DrawColor = FLinearColor::White, ETextHorzPos::Type TextHorzAlignment = ETextHorzPos::Left, ETextVertPos::Type TextVertAlignment = ETextVertPos::Top
		, const FFontRenderInfo& RenderInfo
#if CPP
		= FFontRenderInfo()
#endif
		);

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
	virtual void DrawTexture(UTexture* Texture, float X, float Y, float Width, float Height, float MaterialU = 0.0f, float MaterialV = 0.0f, float MaterialUL = 1.0, float MaterialVL = 1.0f,float DrawOpacity = 1.0f, FLinearColor DrawColor = FLinearColor::White, FVector2D RenderOffset = FVector2D(0.0f, 0.0f), float Rotation = 0, FVector2D RotPivot = FVector2D(0.5f, 0.5f));

	/**
	 * Draws a material on the screen.
	 * @param Texture		The material to draw on the screen
	 * @param X				The X position relative to the widget
	 * @param Y				The Y position relative to the widget
	 * @param Width			The Width (in pixels) of the image to display.  If the widget is scaled, this value will be as well.
	 * @param Height		The Height (in pixels) of the image to display.  If the widget is scaled, this value will be as well.
	 * @param MaterialU		The normalize left position within the source texture
	 * @param MaterialV		The normalize top position within the source texture
	 * @param MaterialUL	The normalize right position within the source texture ````
	 * @param MaterialVL	The normalize bottom position within the source texture
	 * @param DrawColor		The Color to use when drawing this texture
	 * @param RenderOffset	When drawing this image, these offsets will be applied to the position.  They are normalized to the width and height of the image being draw.
	 * @param Rotation		The widget will be rotated by this value.  In Degrees
	 * @param RotPivot		The point at which within the image that the rotation will be around
	 **/
	UFUNCTION(BlueprintCallable, Category="Widgets")
	virtual void DrawMaterial( UMaterialInterface* Material, float X, float Y, float Width, float Height, float MaterialU, float MaterialV, float MaterialUWidth, float MaterialVHeight, float DrawOpacity = 1.0f, FLinearColor DrawColor = FLinearColor::White, FVector2D RenderOffset = FVector2D(0.0f, 0.0f), float Rotation=0, FVector2D RotPivot = FVector2D(0.5f, 0.5f));

protected:
	// Draws any render objects associated with this widget.  NOTE: If a blueprint overrides Draw it needs to call DrawAllRenderObjects
	UFUNCTION(BlueprintCallable, Category="Render Objects")
	virtual void DrawAllRenderObjects(float DeltaTime, FVector2D DrawOffset);

	/**	These are the HUDRenderObject render functions.  They will take whatever is defined by the object and display it. **/
	UFUNCTION(BlueprintCallable, Category="Render Objects")
	virtual void RenderObj_Texture(FHUDRenderObject_Texture& TextureObject, FVector2D DrawOffset = FVector2D(0,0));

	// Render a Texture object at a given position on the screen.
	UFUNCTION(BlueprintCallable, Category="Render Objects")
	virtual void RenderObj_TextureAt(FHUDRenderObject_Texture& TextureObject, float X, float Y, float Width, float Height);

	UFUNCTION(BlueprintCallable, Category="Render Objects")
	virtual FVector2D RenderObj_Text(FHUDRenderObject_Text& TextObject, FVector2D DrawOffset = FVector2D(0,0));

	UFUNCTION(BlueprintCallable, Category="Render Objects")
	virtual FVector2D RenderObj_TextAt(FHUDRenderObject_Text& TextObject, float X, float Y);
	
	// Get any scaling overrides.  
	virtual float GetDrawScaleOverride();

private:
	// This is a sorted list of all RenderObjects in this widget.  
	UPROPERTY()
	TArray<UStructProperty*> RenderObjectList;
};

