// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTScoreboard.h"
#include "UTHUDWidget.h"
#include "UTHUDWidget_ReplayTimeSlider.h"
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

	// The tiny font to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* TinyFont;

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
	class UFont* HugeFont;

	// The score font to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* ScoreFont;

	// The font that only contains numbers
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* NumberFont;

	/** Set when HUD fonts have been cached. */
	UPROPERTY(BlueprintReadOnly, Category = HUD)
		bool bFontsCached;

	/** Cache fonts this HUD will use */
	virtual void CacheFonts();

	// The Global Opacity for Hud Widgets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	float WidgetOpacity;

	/** Cached reference to the spectator message widget. */
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	class UUTHUDWidget_Spectator* SpectatorMessageWidget;

	/** Cached reference to the replay time slider widget. */
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	class UUTHUDWidget_ReplayTimeSlider* ReplayTimeSliderWidget;

	class UUTHUDWidget_ReplayTimeSlider* GetReplayTimeSlider() { return ReplayTimeSliderWidget; }

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	class UUTHUDWidget_SpectatorSlideOut* SpectatorSlideOutWidget;

	class UUTHUDWidget_SpectatorSlideOut* GetSpectatorSlideOut() { return SpectatorSlideOutWidget; }

	// The Global Opacity for Hud Widgets
	UPROPERTY(BlueprintReadWrite, Category = HUD)
	float LastPickupTime;

	/** Crosshair asset pointer */
	UTexture2D* DefaultCrosshairTex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		UTexture2D* HUDAtlas;

	UPROPERTY(EditAnywhere, Category = "Scoreboard")
		FVector2D TeamIconUV[2];

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

	// This is a list of hud widgets that are defined in DefaultGame.ini to be loaded for spectators.  NOTE: you can use 
	// embedded JSON to set their position.  See BuildHudWidget().
	UPROPERTY(Config = Game)
		TArray<FString> SpectatorHudWidgetClasses;

	UPROPERTY()
		bool bHaveAddedSpectatorWidgets;

	virtual void AddSpectatorWidgets();

	// Add any of the blueprint based hud widgets
	virtual void BeginPlay();

	virtual void PostInitializeComponents();

	// Loads the subclass of a hudwidget using just the resource name.  
	TSubclassOf<UUTHUDWidget> ResolveHudWidgetByName(const TCHAR* ResourceName);

	// Creates and adds a hud widget
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = Hud)
	virtual UUTHUDWidget* AddHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass);

	UUTHUDWidget* FindHudWidgetByClass(TSubclassOf<UUTHUDWidget> SearchWidgetClass, bool bExactClass = false);

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

	UPROPERTY(BlueprintReadWrite, Category ="HUD")
	uint32 bForceScores:1;

	/** Creates the scoreboard */
	virtual void CreateScoreboard(TSubclassOf<class UUTScoreboard> NewScoreboardClass);
	
	UTexture2D* OldHudTexture;

	virtual void PawnDamaged(FVector HitLocation, int32 DamageAmount, bool bFriendlyFire);
	virtual void DrawDamageIndicators();

	/** called when PlayerOwner caused damage to HitPawn */
	virtual void CausedDamage(APawn* HitPawn, int32 Damage);

	virtual class UFont* GetFontFromSizeIndex(int32 FontSize) const;

	/**
	 *	@Returns the base color for this hud.  All HUD widgets will start with this.
	 **/
	virtual FLinearColor GetBaseHUDColor();

	virtual void ShowDebugInfo(float& YL, float& YPos) override;

	/** get player state for which to display scoring info. */
	virtual AUTPlayerState* GetScorerPlayerState();

	virtual void NotifyMatchStateChange();

	inline UUTScoreboard* GetScoreboard() const
	{
		return MyUTScoreboard;
	}

public:
	// This is the base HUD opacity level used by HUD Widgets RenderObjects
	UPROPERTY(globalconfig)
	float HUDWidgetOpacity;

	// HUD widgets that have borders will use this opacity value when rendering.
	UPROPERTY(globalconfig)
	float HUDWidgetBorderOpacity;

	// HUD widgets that have background slates will use this opacity value when rendering.
	UPROPERTY(globalconfig)
	float HUDWidgetSlateOpacity;

	// This is a special opacity value used by just the Weapon bar.  When the weapon bar isn't in use, this opacity value will be multipled in
	UPROPERTY(globalconfig)
	float HUDWidgetWeaponbarInactiveOpacity;

	// The weapon bar can get a secondary scale override using this value
	UPROPERTY(globalconfig)
	float HUDWidgetWeaponBarScaleOverride;

	UPROPERTY(globalconfig)
	float HUDWidgetWeaponBarInactiveIconOpacity;

	UPROPERTY(globalconfig)
	float HUDWidgetWeaponBarEmptyOpacity;

	// Allows the user to override the scaling factor for their hud.
	UPROPERTY(globalconfig)
	float HUDWidgetScaleOverride;

	UPROPERTY(globalconfig)
	bool bUseWeaponColors;

	// accessor for CachedTeamColor.  
	FLinearColor GetWidgetTeamColor();

	// These 3 vars are used in a few widgets so we calculate them once in PreDraw so they may be used each frame.
	int32 CurrentPlayerStanding;
	int32 CurrentPlayerScore;
	int32 CurrentPlayerSpread;

	int32 NumActualPlayers;

	UPROPERTY()
	TArray<AUTPlayerState*> Leaderboard;

	// Used to determine which page of the scoreboard we should show
	UPROPERTY()
	int32 ScoreboardPage;

protected:

	// We cache the team color so we only have to look it up once at the start of the render pass
	FLinearColor CachedTeamColor;

	// The current Scoreboard
	UPROPERTY()
	class UUTScoreboard* MyUTScoreboard;

public:
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

	virtual bool HasHudWidget(TSubclassOf<UUTHUDWidget> NewWidgetClass);

protected:
	// Helper function to take a JSON object and try to convert it to the FVector2D.  
	virtual FVector2D JSon2FVector2D(const TSharedPtr<FJsonObject> Vector2DObject, FVector2D Default);

	/** Last time CalcStanding() was run. */
	float CalcStandingTime;

public:

	// Calculates the currently viewed player's standing.  NOTE: Happens once per frame
	void CalcStanding();

	// Takes seconds and converts it to a string
	FText ConvertTime(FText Prefix, FText Suffix, int32 Seconds, bool bForceHours = true, bool bForceMinutes = true, bool bForceTwoDigits = true) const;

	// Creates a suffix string based on a value (st, nd, rd, th).
	FText GetPlaceSuffix(int32 Value);

	void DrawString(FText Text, float X, float Y, ETextHorzPos::Type HorzAlignment, ETextVertPos::Type VertAlignment, UFont* Font, FLinearColor Color, float Scale=1.0, bool bOutline=false);
	void DrawNumber(int32 Number, float X, float Y, FLinearColor Color, float GlowOpacity, float Scale, int32 MinDigits=0, bool bRightAlign=false);

	virtual float GetCrosshairScale();
	virtual FLinearColor GetCrosshairColor(FLinearColor InColor) const;

private:
	UTexture2D* DamageIndicatorTexture;

protected:
	TArray<UTexture2D*> FlagTextures;

public:
	UPROPERTY(Config)
	TArray<FFlagInfo> FlagList;

	UTexture2D* ResolveFlag(int32 FlagID, int32& X, int32& Y);

};

