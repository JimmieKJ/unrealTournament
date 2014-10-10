// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTScoreboard.h"
#include "UTHUDWidget.h"
#include "Json.h"
#include "UTHUD.generated.h"

const uint32 MAX_DAMAGE_INDICATORS = 3;		// # of damage indicators on the screen at any one time
const float DAMAGE_FADE_DURATION = 1.0f;	// How fast a damage indicator fades out

USTRUCT()
struct FDamageHudIndicator
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float RotationAngle;

	UPROPERTY()
	float DamageAmount;

	UPROPERTY()
	float FadeTime;

	UPROPERTY()
	bool bFriendlyFire;

	FDamageHudIndicator()
		: RotationAngle(0.0f), DamageAmount(0.0f), FadeTime(0.0f), bFriendlyFire(false)
	{
	}
};

UCLASS(Config=Game)
class UNREALTOURNAMENT_API AUTHUD : public AHUD
{
	GENERATED_UCLASS_BODY()

public:
	// Holds the UTPlayerController that owns this hud.  NOTE: This is only valid during the render phase
	class AUTPlayerController* UTPlayerOwner;

	// Holds the list of all hud widgets that are currently active
	UPROPERTY(Transient)
	TArray<class UUTHUDWidget*> HudWidgets;

	// Holds a list of HudWidgets that manage the messaging system
	TMap<FName, class UUTHUDWidgetMessage*> HudMessageWidgets;

	// Holds a list of hud widgets to load.  These are hardcoded widgets that can't be changed via the ini and are stored as strings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	TArray<FString> HudWidgetClasses;

	// The Small font to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* SmallFont;

	// The Med font to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* MediumFont;

	// The Large font to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* LargeFont;

	// The "extreme" font to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* ExtremeFont;

	// The font that only contains numbers
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* NumberFont;

	// The Global Opacity for Hud Widgets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	float WidgetOpacity;

	/** Crosshair asset pointer */
	UTexture2D* DefaultCrosshairTex;

	/** last time we hit an enemy in LOS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	float LastConfirmedHitTime;

	// Active Damage Indicators.  NOTE: if FadeTime == 0 then it's not in use
	UPROPERTY()
	TArray<struct FDamageHudIndicator> DamageIndicators;

	// This is a list of hud widgets that are defined in DefaultGame.ini to be loaded.  NOTE: you can use 
	// embedded JSON to set their position.  See BuildHudWidget().
	UPROPERTY(Config = Game)
	TArray<FString> RequiredHudWidgetClasses;

	// Add any of the blueprint based hud widgets
	virtual void BeginPlay();

	virtual void PostInitializeComponents();

	// Loads the subclass of a hudwidget using just the resource name.  
	TSubclassOf<UUTHUDWidget> ResolveHudWidgetByName(const TCHAR* ResourceName);

	// Creates and adds a hud widget
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = Hud)
	virtual UUTHUDWidget* AddHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass);

	UUTHUDWidget* FindHudWidgetByClass(TSubclassOf<UUTHUDWidget> SearchWidgetClass);

	// We override PostRender so that we can cache bunch of vars that need caching.
	virtual void PostRender();

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

	/** Receive a localized message from somewhere to be displayed */
	virtual void ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject = NULL);
	
	UFUNCTION(BlueprintCallable, Category="HUD")
	virtual void ToggleScoreboard(bool bShow);

	UPROPERTY(BlueprintReadOnly, Category="HUD")
	uint32 bShowScores:1;

	/** Creates the scoreboard */
	virtual void CreateScoreboard(TSubclassOf<class UUTScoreboard> NewScoreboardClass);
	
	UTexture2D* OldHudTexture;

	virtual void PawnDamaged(FVector HitLocation, int32 DamageAmount, TSubclassOf<UDamageType> DamageClass, bool bFriendlyFire);
	virtual void DrawDamageIndicators();

	/** called when PlayerOwner caused damage to HitPawn */
	virtual void CausedDamage(APawn* HitPawn, int32 Damage);

	virtual class UFont* GetFontFromSizeIndex(int32 FontSize) const;

	/**
	 *	@Returns the base color for this hud.  All HUD widgets will start with this.
	 **/
	virtual FLinearColor GetBaseHUDColor();

	virtual void ShowDebugInfo(float& YL, float& YPos) override;

protected:

	// The current Scoreboard
	UPROPERTY()
	class UUTScoreboard* MyUTScoreboard;

	// Takes a raw widget string and tries to build a widget from it.  It supports embedded JSON objects that define the widget as follows:
	//
	// { "Classname" : "//path.to.unreal.object",
	//		"ScreenPosition" : { "X" : ###, "Y" : ### },
	//		"Origin" : { "X" : ###, "Y" : ### },
	//	    "Position" : { "X" : ###, "Y" : XXX }
	// }
	// 
	// NOTE the 3 position vars are not required and it will default to their original values.  So :
	//
	// {"Classname":"/Script/UnrealTournament.UTHUDWidget_GameClock", "ScreenPosition" : { "Y" : 0.5}}
	//
	// Would display the GameClock widget 1/2 way down the screen on the left edge.  See UTHUDWidget.h for a description of the difference between ScreenPosition, Origin and Position.
	//
	// Finally, you can use just the unreal path without JSON to load an object.  See DefaultGame.ini for examples.

	virtual void BuildHudWidget(FString NewWidgetString);

	// Helper function to take a JSON object and try to convert it to the FVector2D.  
	virtual FVector2D JSon2FVector2D(const TSharedPtr<FJsonObject> Vector2DObject, FVector2D Default);

public:

	FText ConvertTime(FText Prefix, FText Suffix, int Seconds) const;

	void DrawString(FText Text, float X, float Y, ETextHorzPos::Type HorzAlignment, ETextVertPos::Type VertAlignment, UFont* Font, FLinearColor Color, float Scale=1.0, bool bOutline=false);
	void DrawNumber(int Number, float X, float Y, FLinearColor Color, float GlowOpacity, float Scale, int MinDigits=0, bool bRightAlign=false);



private:
	UTexture2D* DamageIndicatorTexture;
};

