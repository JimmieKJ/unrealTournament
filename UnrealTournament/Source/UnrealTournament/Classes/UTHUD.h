// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUD.generated.h"

UCLASS()
class AUTHUD : public AHUD
{
	GENERATED_UCLASS_BODY()

public:
	// Holds the UTPlayerController that owns this hud.  NOTE: This is only valid during the render phase
	class AUTPlayerController* UTPlayerOwner;

	// Holds the character.  NOTE: This is only valid during the render phase
	class AUTCharacter* UTCharacterOwner;

	// Holds the list of all hud widgets that are currently active
	UPROPERTY(Transient)
	TArray<class UUTHUDWidget*> HudWidgets;

	// Holds a list of HudWidgets that manage the messaging system
	TMap<FName, class UUTHUDWidgetMessage*> HudMessageWidgets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	TArray<TSubclassOf<UUTHUDWidget> > HudWidgetClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* MediumFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* LargeFont;

	// The Global Opacity for Hud Widgets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	float WidgetOpacity;


	// Add any of the blueprint based hud widgets
	virtual void BeginPlay();

	// Creates and adds a hud widget
	virtual void AddHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass);

	// We override PostRender so that we can cache bunch of vars that need caching.
	virtual void PostRender();

	/** Primary draw call for the HUD */
	virtual void DrawHUD() OVERRIDE;

	/** Receive a localized message from somewhere to be displayed */
	virtual void ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject = NULL);

protected:

	UTexture2D* OldHudTexture;

	// TEMP : Convert seconds to a time string.
	FText TempConvertTime(int Seconds);

	// TEMP: Until the new Hud system comes online, quickly draw a string to the screen.  This will be replaced soon.
	void TempDrawString(FText Text, float X, float Y, ETextHorzPos::Type TextPosition, UFont* Font, FLinearColor Color);

public:
	void TempDrawNumber(int Number, float X, float Y, FLinearColor Color, float GlowOpacity, float Scale);

private:
	/** Crosshair asset pointer */
	UTexture2D* CrosshairTex;

};

