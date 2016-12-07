// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTScoreboard.h"
#include "UTDamageType.h"
#include "UTHUDWidget.h"
#include "UTHUDWidget_ReplayTimeSlider.h"
#include "Json.h"
#include "UTHUD.generated.h"

const uint32 MAX_DAMAGE_INDICATORS = 3;				// # of damage indicators on the screen at any one time
const float DAMAGE_FADE_DURATION = 1.0f;			// How fast a damage indicator fades out
const float MAX_MY_DAMAGE_BOUNCE_TIME = 1.25f;		// How fast will the numbers bounce in to the screen.
const float MAX_TALLY_FADE_TIME = 0.6f;				// How fast should the talley numbers fade in

class UUTProfileSettings;

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

USTRUCT()
struct FEnemyDamageNumber
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	APawn* DamagedPawn;

	UPROPERTY()
		float DamageTime;

	UPROPERTY()
	uint8 DamageAmount;

	UPROPERTY()
	FVector WorldPosition;

	UPROPERTY()
	float Scale;

	FEnemyDamageNumber()
		: DamagedPawn(NULL), DamageTime(0.f), DamageAmount(0), WorldPosition(FVector(0.f)), Scale(1.f)
	{
	}

	FEnemyDamageNumber(APawn* InPawn, float InTime, uint8 InDamage, FVector InLoc, float InScale) : DamagedPawn(InPawn), DamageTime(InTime), DamageAmount(InDamage), WorldPosition(InLoc), Scale(InScale) {};
};

class UUTRadialMenu;
class UUTRadialMenu_Coms;
class UUTRadialMenu_WeaponWheel;

class UUTUMGHudWidget;

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

	// The chat font to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
		class UFont* ChatFont;

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

	// The font that only contains numbers
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	class UFont* NumberFont;

	/** Set when HUD fonts have been cached. */
	UPROPERTY(BlueprintReadOnly, Category = HUD)
		bool bFontsCached;

	/** Cache fonts this HUD will use */
	virtual void CacheFonts();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
		FText ScoreMessageText;

	virtual float DrawWinConditions(UFont* InFont, float XPos, float YPos, float ScoreWidth, float RenderScale, bool bCenterMessage, bool bSkipDrawing = false);

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

	UPROPERTY(BlueprintReadOnly, Category = HUD)
		class UUTHUDWidgetMessage_KillIconMessages* KillIconWidget;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
		class UUTHUDWidgetAnnouncements* AnnouncementWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUD)
		FVector2D ScoreboardKillFeedPosition;

	class UUTHUDWidget_SpectatorSlideOut* GetSpectatorSlideOut() { return SpectatorSlideOutWidget; }

	/** Damage values caused by viewed player recently. */
	UPROPERTY(BlueprintReadWrite, Category = HUD)
		TArray<FEnemyDamageNumber> DamageNumbers;

	UPROPERTY()
		bool bDrawDamageNumbers;

	/** Draw in screen space damage recently applied by this player to other characters. */
	virtual void DrawDamageNumbers();

	/** Used when changing viewed player. */
	virtual void ClearIndicators();

	UPROPERTY(BlueprintReadWrite, Category = HUD)
	float LastPickupTime;

	/** Last time player owning this HUD killed someone. */
	UPROPERTY(BlueprintReadWrite, Category = HUD)
	float LastKillTime;

	UPROPERTY(BlueprintReadWrite, Category = HUD)
		int32 LastMultiKillCount;

	/** Last time player owning this HUD picked up a flag. */
	UPROPERTY(BlueprintReadWrite, Category = HUD)
	float LastFlagGrabTime;

	/** sound played when player gets a kill */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	USoundBase* KillSound;

	FTimerHandle PlayKillHandle;

	/** Queue a kill notification. */
	virtual void NotifyKill(APlayerState* POVPS, APlayerState* KillerPS, APlayerState* VictimPS);

	/** Play kill notification sound and icon. */
	virtual void PlayKillNotification();

	/** Crosshair asset pointer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UTexture2D* DefaultCrosshairTex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UTexture2D* HUDAtlas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UTexture2D* HUDAtlas3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoreboard")
	UTexture2D* ScoreboardAtlas;

	UPROPERTY(EditAnywhere, Category = "Scoreboard")
	FVector2D TeamIconUV[4];

	/** last time we hit an enemy in LOS */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	float LastConfirmedHitTime;

	/** Damage given in that hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	int32 LastConfirmedHitDamage;

	/** if it was a kill,  this will be true */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	bool LastConfirmedHitWasAKill;

	// Active Damage Indicators.  NOTE: if FadeTime == 0 then it's not in use
	UPROPERTY()
	TArray<struct FDamageHudIndicator> DamageIndicators;

	/** full screen material drawn when taking damage, intensity based on damage amount */
	UPROPERTY()
	UMaterialInterface* DamageScreenMat;
	UPROPERTY()
	UMaterialInstanceDynamic* DamageScreenMID;

	// This is a list of hud widgets that are defined in DefaultGame.ini to be loaded.  NOTE: you can use 
	// embedded JSON to set their position.  See BuildHudWidget().
	UPROPERTY(Config = Game)
	TArray<FString> RequiredHudWidgetClasses;

	// This is a list of hud widgets that are defined in DefaultGame.ini to be loaded for spectators.  NOTE: you can use 
	// embedded JSON to set their position.  See BuildHudWidget().
	UPROPERTY(Config = Game)
		TArray<FString> SpectatorHudWidgetClasses;

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

	/** Whether scoreboard should be displayed this tick. */
	virtual bool ScoreboardIsUp();

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

	/** Receive a localized message from somewhere to be displayed */
	virtual void ReceiveLocalMessage(TSubclassOf<class UUTLocalMessage> MessageClass, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, uint32 MessageIndex, FText LocalMessageText, UObject* OptionalObject = NULL);
	
	UFUNCTION(BlueprintCallable, Category="HUD")
	virtual void ToggleScoreboard(bool bShow);

	UPROPERTY(BlueprintReadOnly, Category="HUD")
	uint32 bShowScores:1;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
		uint32 bShowScoresWhileDead : 1;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
		uint32 bDrawMinimap : 1;

	/** icon for player on the minimap (rotated BG that indicates direction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
		UTexture2D* PlayerMinimapTexture;

	/** drawn over selected player on the minimap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
		UTexture2D* SelectedPlayerTexture;

	/** background for help text over map */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
		FCanvasIcon SpawnHelpTextBG;

	UPROPERTY(BlueprintReadWrite, Category ="HUD")
	uint32 bForceScores:1;

	virtual void PawnDamaged(uint8 ShotDirYaw, int32 DamageAmount, bool bFriendlyFire, TSubclassOf<class UDamageType> DamageTypeClass = nullptr);
	virtual void DrawDamageIndicators();

	/** called when PlayerOwner caused damage to HitPawn */
	virtual void CausedDamage(APawn* HitPawn, int32 Damage);

	virtual class UFont* GetFontFromSizeIndex(int32 FontSize) const;

	/**
	 *	@Returns the base color for this hud.  All HUD widgets will start with this.
	 **/
	virtual FLinearColor GetBaseHUDColor();

	virtual void ShowDebugInfo(float& YL, float& YPos) override;

	UPROPERTY()
		bool bShowUTHUD;

	virtual void ShowHUD() override;

	/** get player state for which to display scoring info. */
	virtual AUTPlayerState* GetScorerPlayerState();

	virtual void NotifyMatchStateChange();

	FTimerHandle MatchSummaryHandle;

	virtual void OpenMatchSummary();

	inline UUTScoreboard* GetScoreboard() const
	{
		return MyUTScoreboard;
	}

protected:
	UUTProfileSettings* CachedProfileSettings;

public:
	// This is the base HUD opacity level used by HUD Widgets RenderObjects
	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetHUDWidgetOpacity();

	// HUD widgets that have borders will use this opacity value when rendering.
	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetHUDWidgetBorderOpacity();

	// HUD widgets that have background slates will use this opacity value when rendering.
	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetHUDWidgetSlateOpacity();

	// This is a special opacity value used by just the Weapon bar.  When the weapon bar isn't in use, this opacity value will be multipled in
	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetHUDWidgetWeaponbarInactiveOpacity();

	// The weapon bar can get a secondary scale override using this value
	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetHUDWidgetWeaponBarScaleOverride();

	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetHUDWidgetWeaponBarInactiveIconOpacity();

	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetHUDWidgetWeaponBarEmptyOpacity();

	/** Set true to force weapon bar to immediately update. */
	UPROPERTY()
	bool bHUDWeaponBarSettingChanged;

	UPROPERTY()
		float MiniMapIconAlpha;

	UPROPERTY()
		float MiniMapIconMuting;

	// Allows the user to override the scaling factor for their hud.
	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetHUDWidgetScaleOverride();

	// Allows the user to override the scaling factor for their hud.
	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetHUDMessageScaleOverride();

	// Allows the user to override the scaling factor for their hud.

	UFUNCTION(BlueprintCallable, Category=HUD)
	bool GetUseWeaponColors();

	UFUNCTION(BlueprintCallable, Category=HUD)
	bool GetDrawChatKillMsg();

	UFUNCTION(BlueprintCallable, Category=HUD)
	bool GetDrawCenteredKillMsg();

	UFUNCTION(BlueprintCallable, Category=HUD)
	bool GetDrawHUDKillIconMsg();

	UFUNCTION(BlueprintCallable, Category=HUD)
	bool GetPlayKillSoundMsg();

	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetQuickStatsAngle();

	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetQuickStatsDistance();

	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetQuickStatScaleOverride();
	
	UFUNCTION(BlueprintCallable, Category=HUD)
	FName GetQuickStatsType();

	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetQuickStatsBackgroundAlpha();

	UFUNCTION(BlueprintCallable, Category=HUD)
	float GetQuickStatsForegroundAlpha();

	UFUNCTION(BlueprintCallable, Category=HUD)
	bool GetQuickStatsHidden();

	UFUNCTION(BlueprintCallable, Category=HUD)
	bool GetQuickInfoHidden();

	UFUNCTION(BlueprintCallable, Category = HUD)
		bool GetHealthArcShown();

	UFUNCTION(BlueprintCallable, Category = HUD)
		float GetHealthArcRadius();

	// accessor for CachedTeamColor.  
	FLinearColor GetWidgetTeamColor();

	// These 3 vars are used in a few widgets so we calculate them once in PreDraw so they may be used each frame.
	int32 CurrentPlayerStanding;
	int32 CurrentPlayerScore;
	int32 CurrentPlayerSpread;

	int32 NumActualPlayers;

	UPROPERTY()
	TArray<AUTPlayerState*> Leaderboard;

	virtual bool ShouldDrawMinimap();

	// The current Scoreboard
	UPROPERTY()
		class UUTScoreboard* MyUTScoreboard;

protected:

	// We cache the team color so we only have to look it up once at the start of the render pass
	FLinearColor CachedTeamColor;


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

	/** Find and return requested key mapping. */
	FText FindKeyMappingTo(FName ActionName);

	/** Update displayed key bindings on HUD */
	virtual void UpdateKeyMappings(bool bForceUpdate);

	UPROPERTY()
		bool bKeyMappingsSet;

	UPROPERTY()
		FText RallyLabel;

	UPROPERTY()
		FText BoostLabel;

	UPROPERTY()
		FText ShowScoresLabel;

protected:
	// Helper function to take a JSON object and try to convert it to the FVector2D.  
	virtual FVector2D JSon2FVector2D(const TSharedPtr<FJsonObject> Vector2DObject, FVector2D Default);

	/** Last time CalcStanding() was run. */
	float CalcStandingTime;

public:

	// Calculates the currently viewed player's standing.  NOTE: Happens once per frame
	void CalcStanding();

	// Takes seconds and converts it to a string
	FText ConvertTime(FText Prefix, FText Suffix, int32 Seconds, bool bForceHours = false, bool bForceMinutes = true, bool bForceTwoDigits = true) const;

	// Creates a suffix string based on a value (st, nd, rd, th).
	FText GetPlaceSuffix(int32 Value);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUDText)
	FText TimerHours;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUDText)
		FText TimerMinutes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUDText)
		FText TimerSeconds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUDText)
		FText SuffixFirst;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUDText)
		FText SuffixSecond;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUDText)
		FText SuffixThird;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = HUDText)
		FText SuffixNth;

	UPROPERTY()
		FText BuildText;

	UPROPERTY()
		float BuildTextWidth;

	void DrawString(FText Text, float X, float Y, ETextHorzPos::Type HorzAlignment, ETextVertPos::Type VertAlignment, UFont* Font, FLinearColor Color, float Scale=1.0, bool bOutline=false);
	void DrawNumber(int32 Number, float X, float Y, FLinearColor Color, float GlowOpacity, float Scale, int32 MinDigits=0, bool bRightAlign=false);

	virtual float GetCrosshairScale();
	virtual FLinearColor GetCrosshairColor(FLinearColor InColor) const;

private:
	UTexture2D* DamageIndicatorTexture;

public:
	UFUNCTION(BlueprintCallable, Category = HUD)
	UTexture2D* ResolveFlag(AUTPlayerState* PS, FTextureUVs& UV);

	/**Returns the necessary input mode for the hud this tick*/
	UFUNCTION(BlueprintNativeEvent)
	EInputMode::Type GetInputMode() const;

	// Holds a list of all loaded crosshairs.
	UPROPERTY()
	TMap<FName, UUTCrosshair*> Crosshairs;

	/** called by PlayerController (locally) when clicking mouse while crosshair is selected
	 * return true to override default behavior
	 */
	virtual bool OverrideMouseClick(FKey Key, EInputEvent EventType)
	{
		return false;
	}

	/** render target for the minimap */
	UPROPERTY()
	UCanvasRenderTarget2D* MinimapTexture;

	/** Minimap Actor whose icon the mouse pointer is hovering over last time we checked */
	UPROPERTY(BlueprintReadOnly)
		AActor* LastHoveredActor;

	/** most recent time LastHoveredActor changed - NOTE: This is in RealTimeSeconds! */
	UPROPERTY(BlueprintReadOnly)
		float LastHoveredActorChangeTime;

	/** transformation matrix from world locations to minimap locations */
	FMatrix MinimapTransform;

	/** Offset when displaying minimap snug to an edge. */
	FVector2D MinimapOffset;
	
	/** map transform for rendering on screen (used to convert clicks to map locations) */
	FMatrix MapToScreen;
	
	/** draw the static pre-rendered portions of the minimap to the MinimapTexture */
	UFUNCTION()
	virtual void UpdateMinimapTexture(UCanvas* C, int32 Width, int32 Height);

	virtual void CreateMinimapTexture();
	/** draws minimap; creates and updates the minimap render-to-texture if it hasn't been already
	 * Sets MapToScreen so subclasses can easily override and use WorldToMapToScreen() to place icons
	 * @param DrawColor: color to use when drawing the minimap texture
	 * @param MapSize: on-screen size of the map (square)
	 * @param DrawPos: draw coordinates
	 */
	UFUNCTION(BlueprintCallable, Category = HUD)
	virtual void DrawMinimap(const FColor& DrawColor, float MapSize, FVector2D DrawPos);

	/** Draw a minimap icon that is a included in a large texture. */
	UFUNCTION(BlueprintCallable, Category = HUD)
		virtual void DrawMinimapIcon(UTexture2D* Texture, FVector2D Pos, FVector2D DrawSize, FVector2D UV, FVector2D UVL, FLinearColor DrawColor, bool bDropShadow);

	virtual void DrawMinimapSpectatorIcons();

	/** Whether minimap should be vertically inverted from default drawing orientation. */
	UPROPERTY(BlueprintReadWrite, Category = HUD)
		bool bInvertMinimap;

	virtual bool ShouldInvertMinimap();

protected:
	/** calculates MinimapTransform from the given level bounding box */
	virtual void CalcMinimapTransform(const FBox& LevelBox, int32 MapWidth, int32 MapHeight);
	/** transform InPos to coordinates corresponding to the map's position on the screen, i.e. transform world -> map then map -> screen
	 * note: the transform is updated via DrawMinimap(), so if that isn't be called the values may not be correct
	 */
	FVector2D WorldToMapToScreen(const FVector& InPos) const
	{
		return FVector2D(MapToScreen.TransformPosition(MinimapTransform.TransformPosition(InPos)));
	}

	void DrawWatermark();

public:
	UFUNCTION()
	virtual void ClientRestart();

	virtual void DrawKillSkulls();

	bool VerifyProfileSettings();
	virtual void Destroyed();

	virtual bool ProcessInputAxis(FKey Key, float Delta);
	virtual bool ProcessInputKey(FKey Key, EInputEvent EventType);

	bool bShowComsMenu;
	virtual void ToggleComsMenu(bool bShow);

	bool bShowWeaponWheel;
	virtual void ToggleWeaponWheel(bool bShow);

	bool bShowVoiceDebug;

protected:

	UPROPERTY()
	TArray<UUTRadialMenu*> RadialMenus;
	UPROPERTY()
	UUTRadialMenu_Coms* ComsMenu;
	UPROPERTY()
	UUTRadialMenu_WeaponWheel* WeaponWheel;

public:

	/**
	 *	returns true if a given umg widget is active on the stack
	 **/
	bool IsUMGWidgetActive(TWeakObjectPtr<UUTUMGHudWidget> TestWidget);

	/**
	 *	Activate a UMG HUD widget and display it over the HUD
	 **/
	virtual TWeakObjectPtr<class UUTUMGHudWidget> ActivateUMGHudWidget(FString UMGHudWidgetClassName, bool bUnique = true);
	virtual void ActivateActualUMGHudWidget(TWeakObjectPtr<UUTUMGHudWidget> WidgetToActivate);

	/**
	 *	Deactivates a UMG HUD widget that is already active
	 **/
	virtual void DeactivateUMGHudWidget(FString UMGHudWidgetClassName);
	virtual void DeactivateActualUMGHudWidget(TWeakObjectPtr<UUTUMGHudWidget> WidgetToDeactivate);

	/**
	 *	Look up the crosshair information for a given weapon.  Returns the default object for the crosshair and passes out 
	 *  the customization info.
	 **/
	UFUNCTION()
	UUTCrosshair* GetCrosshairForWeapon(FName WeaponCustomizationTag, FWeaponCustomizationInfo& outWeaponCustomizationInfo);

	virtual void DrawActorOverlays(FVector Viewpoint, FRotator ViewRotation) override;

	/** Holds the atlats that make up the base character portraits*/
	UPROPERTY()
	UTexture2D* CharacterPortraitAtlas;

protected:
	TArray<TWeakObjectPtr<UUTUMGHudWidget>> UMGHudWidgetStack;



};

