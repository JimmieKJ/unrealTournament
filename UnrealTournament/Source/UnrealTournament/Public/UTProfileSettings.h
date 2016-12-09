// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
// These are settings that are stored in a remote MCP managed profile.  A copy of them are also stored in the user folder on the local machine
// in case of MCP failure or downtime.

#pragma once

#include "GameFramework/InputSettings.h"
#include "UTPlayerInput.h"
#include "UTPlayerController.h"
#include "UTWeaponSkin.h"
#include "UTProfileSettings.generated.h"


static const uint32 CURRENT_PROFILESETTINGS_VERSION = 37;
static const uint32 VALID_PROFILESETTINGS_VERSION = 32;
static const uint32 WEAPONBAR_FIXUP_VERSION = 33;
static const uint32 COMMENU_FIXUP_VERSION = 34;
static const uint32 ENABLE_DOUBLETAP_DODGE_FIXUP_VERSION = 37;

class UUTLocalPlayer;

UENUM()
namespace EProfileResetType
{
	enum Type
	{
		All,
		System,
		Character,
		Challenges,
		Weapons,
		Social,
		HUD,
		Game,
		Input,
		Binds,
		MAX,
	};
}

UCLASS()
class UNREALTOURNAMENT_API UUTProfileSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UUTProgressionStorage;

public:

	/**
	 *	Apply any settings stored in this profile object
	 **/
	void ApplyAllSettings(UUTLocalPlayer* ProfilePlayer);

	/**
	 *	Allows specific hacks to be applied to the profile based on the version number
	 **/

	bool VersionFixup();

	/**
	 *	Makes sure the GameActions array is up to date
	 **/
	bool ValidateGameActions();

	/**
	 *	Builds the list of default game actions.
	 **/
	void GetDefaultGameActions(TArray<FKeyConfigurationInfo>& outGameActions);

	/**
	 *	Use this function to reset values in the profile to their default state.  NOTE this doesn't save the profile, you have
	 *  to do that manually
	 **/
	void ResetProfile(EProfileResetType::Type SectionToReset);

	// If true, the profile object should be saved on level change.
	bool bNeedProfileWriteOnLevelChange;

	/** Set true to force weapon bar to immediately update. */
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bHUDWeaponBarSettingChanged : 1;

	// If true, we have forced and epic Employee to use the Epic Logo at least once before they changed it.
	UPROPERTY()
	uint32 bForcedToEpicAtLeastOnce : 1;

	// The Version Number
	UPROPERTY()
	uint32 SettingsRevisionNum;

	// When were these settings saved
	UPROPERTY(BlueprintReadOnly, Category = Profile)
	FDateTime SettingsSavedOn;


	// ======================== Character Settings

	// What is the Player name associated with this profile
	UPROPERTY(BlueprintReadOnly, Category = Character)
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	FString HatPath;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	FString LeaderHatPath;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	int32 HatVariant;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	FString EyewearPath;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	int32 EyewearVariant;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	FString GroupTauntPath;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	FString TauntPath;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	FString Taunt2Path;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	FString CharacterPath;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	FName CountryFlag;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	FName Avatar;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	float WeaponBob;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	float ViewBob;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	FLinearColor FFAPlayerColor;

	UPROPERTY(BlueprintReadOnly, Category = Character)
	float PlayerFOV;

	// ======================== Challenge Settings

	UPROPERTY(BlueprintReadOnly, Category = Character)
	int32 LocalXP;

	UPROPERTY(BlueprintReadOnly, Category = Challenges)
	TArray<FUTChallengeResult> ChallengeResults;

	UPROPERTY(BlueprintReadOnly, Category = Challenges)
	TArray<FUTDailyChallengeUnlock> UnlockedDailyChallenges;

	// ======================== Social Settings

	// If true, then the player will not show toasts in game.
	UPROPERTY(BlueprintReadOnly, Category = Social)
	uint32 bSuppressToastsInGame : 1;

	// ======================== Weapon Settings

	UPROPERTY(BlueprintReadOnly, Category = Weapon)
	uint32 bCustomWeaponCrosshairs : 1;

	UPROPERTY(BlueprintReadOnly, Category = Weapon)
	uint32 bSingleCustomWeaponCrosshair : 1;

	UPROPERTY(BlueprintReadOnly, Category = Weapon)
	uint32 bAutoWeaponSwitch : 1;

	UPROPERTY(BlueprintReadOnly, Category = Weapon)
	EWeaponHand WeaponHand;

	UPROPERTY(BlueprintReadOnly, Category = Input)
	FWeaponCustomizationInfo SingleCustomWeaponCrosshair;

	// Holds the mapping values for the weapon wheel.  This will always have 8 entries and each entry is the weapon group to activate on the weapon wheel or -1 for empty
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	TArray<int32> WeaponWheelMapping;

	UPROPERTY()
	TMap<FName, FWeaponCustomizationInfo> WeaponCustomizations;

	UPROPERTY()
	TMap<FName, FString> WeaponSkins;

	// ======================== System Settings

	UPROPERTY(BlueprintReadOnly, Category = System)
	int32 ReplayScreenshotResX;

	UPROPERTY(BlueprintReadOnly, Category = System)
	int32 ReplayScreenshotResY;

	UPROPERTY(BlueprintReadOnly, Category = System)
	uint32 bReplayCustomPostProcess : 1;

	UPROPERTY(BlueprintReadOnly, Category = System)
	float ReplayCustomBloomIntensity;
	
	UPROPERTY(BlueprintReadOnly, Category = System)
	float ReplayCustomDOFAmount;

	UPROPERTY(BlueprintReadOnly, Category = System)
	float ReplayCustomDOFDistance;

	UPROPERTY(BlueprintReadOnly, Category = System)
	float ReplayCustomDOFScale;

	UPROPERTY(BlueprintReadOnly, Category = System)
	float ReplayCustomDOFNearBlur;

	UPROPERTY(BlueprintReadOnly, Category = System)
	float ReplayCustomDOFFarBlur;

	UPROPERTY(BlueprintReadOnly, Category = System)
	float ReplayCustomMotionBlurAmount;

	UPROPERTY(BlueprintReadOnly, Category = System)
	float ReplayCustomMotionBlurMax;

	UPROPERTY(BlueprintReadOnly, Category = System)
	FString MatchmakingRegion;

	UPROPERTY(BlueprintReadOnly, Category = System)
	uint32 bPushToTalk : 1;

	UPROPERTY(BlueprintReadOnly, Category = System)
	uint32 bHearsTaunts : 1;

	// ======================== HUD Settings

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float QuickStatsAngle;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float QuickStatsDistance;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	FName QuickStatsType;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float QuickStatsBackgroundAlpha;	

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float QuickStatsForegroundAlpha;	

	// Are the health/armor/ammo stats hidden in the mini-hud?
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bQuickStatsHidden : 1;

	// Are the powerup/flags hidden in the mini-hud?
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bQuickInfoHidden : 1;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bHealthArcShown : 1;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
		int32 HealthArcRadius;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float QuickStatsScaleOverride;	

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bHideDamageIndicators : 1;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bHidePaperdoll : 1;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bVerticalWeaponBar : 1;

	// This is the base HUD opacity level used by HUD Widgets RenderObjects
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float HUDWidgetOpacity;

	// HUD widgets that have borders will use this opacity value when rendering.
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float HUDWidgetBorderOpacity;

	// HUD widgets that have background slates will use this opacity value when rendering.
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float HUDWidgetSlateOpacity;

	// This is a special opacity value used by just the Weapon bar.  When the weapon bar isn't in use, this opacity value will be multipled in
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float HUDWidgetWeaponbarInactiveOpacity;

	// The weapon bar can get a secondary scale override using this value
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float HUDWidgetWeaponBarScaleOverride;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float HUDWidgetWeaponBarInactiveIconOpacity;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float HUDWidgetWeaponBarEmptyOpacity;

	// Allows the user to override the scaling factor for their hud.
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float HUDWidgetScaleOverride;

	// Allows the user to override the scaling factor for their hud.
	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float HUDMessageScaleOverride;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bUseWeaponColors : 1;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bDrawChatKillMsg : 1;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bDrawCenteredKillMsg : 1;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bDrawHUDKillIconMsg : 1;

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	float HUDTeammateScaleOverride;
	

	UPROPERTY(BlueprintReadOnly, Category = HUD)
	uint32 bPlayKillSoundMsg : 1;

	// ======================== Input Settings

	// Holds all of the actions for the base game.  TODO: Add a mod version so that mods can bind keys easier
	UPROPERTY(BlueprintReadOnly, Category = Input)
	TArray<FKeyConfigurationInfo> GameActions;

	UPROPERTY(BlueprintReadOnly, Category = Input)
	uint32 bEnableMouseSmoothing : 1;

	UPROPERTY(BlueprintReadWrite, Category = Input)
	uint32 bInvertMouse : 1;

	UPROPERTY(BlueprintReadOnly, Category = Input)
	float MouseAcceleration;

	UPROPERTY(BlueprintReadOnly, Category = Input)
	float MouseAccelerationPower;

	UPROPERTY(BlueprintReadOnly, Category = Input)
	float MouseAccelerationMax;

	UPROPERTY(BlueprintReadOnly, Category = Input)
	float DoubleClickTime;

	UPROPERTY(BlueprintReadOnly, Category = Input)
	float MouseSensitivity;

	UPROPERTY(BlueprintReadOnly, Category = Input)
	float MaxDodgeClickTimeValue;

	UPROPERTY(BlueprintReadWrite, Category = Input)
	uint32 bEnableDoubleTapDodge : 1;

	UPROPERTY(BlueprintReadOnly, Category = Input)
	float MaxDodgeTapTimeValue;
	
	UPROPERTY(BlueprintReadOnly, Category = Input)
	uint32 bSingleTapWallDodge:1;

	UPROPERTY(BlueprintReadOnly, Category = Input)
	uint32 bSingleTapAfterJump : 1;

	/** For backwards compatibility, maps to bCrouchTriggersSlide. */
	UPROPERTY(BlueprintReadOnly, Category = Input)
	uint32 bAllowSlideFromRun : 1;

public:

	// Accessor functions.  NOTE: If you add a function here, please make sure it's blueprint callable.

	// Returns the current name of the player
	UFUNCTION(BlueprintCallable, Category = Character)
	FString GetPlayerName() { return PlayerName; };

	// Given a wepaon, look up the Weapon Group information for it.
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void GetWeaponGroup(AUTWeapon* WeaponClass, int32& WeaponGroup, int32& GroupPriority);

	// Performs the actual lookup of a weapon customization tag
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void GetWeaponCustomization(FName WeaponCustomizationTag, FWeaponCustomizationInfo& outWeaponCustomizationInfo);

	// Returns the customization info for a given weapon
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void GetWeaponCustomizationForWeapon(AUTWeapon* Weapon, FWeaponCustomizationInfo& outWeaponCustomizationInfo);

	// Returns the weapon skin for a given weapon
	UFUNCTION(BlueprintCallable, Category = Weapon)
	FString GetWeaponSkinClassname(AUTWeapon* Weapon);

	// Apply the keyboard bindings to the input system.  
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void ApplyInputSettings(UUTLocalPlayer* ProfilePlayer);

	const FKeyConfigurationInfo* FindGameAction(FName SearchTag);
	const FKeyConfigurationInfo* FindGameAction(const FString& SearchTag);

	void ExportKeyBinds();
	void ImportKeyBinds();

	// I'm putting this in the profile not progression because we don't want to save progression locally.  Progression requires
	// you to be connectred, but we need this for the intial onboarding process.

	UPROPERTY(BlueprintReadOnly, Category = Onboarding)
	int32 TutorialMask;

	UPROPERTY(BlueprintReadOnly, Category = Onboarding)
	uint32 SkipOnboarding : 1;

};
