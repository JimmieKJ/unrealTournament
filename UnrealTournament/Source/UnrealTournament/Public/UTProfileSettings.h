// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
// These are settings that are stored in a remote MCP managed profile.  A copy of them are also stored in the user folder on the local machine
// in case of MCP failure or downtime.

#pragma once

#include "GameFramework/InputSettings.h"
#include "UTPlayerInput.h"
#include "UTPlayerController.h"
#include "UTProfileSettings.generated.h"

static const uint32 VALID_PROFILESETTINGS_VERSION = 6;
static const uint32 EMOTE_TO_TAUNT_PROFILESETTINGS_VERSION = 6;
static const uint32 TAUNTFIXUP_PROFILESETTINGS_VERSION = 7;
static const uint32 SPECTATING_FIXUP_PROFILESETTINGS_VERSION = 8;
static const uint32 CURRENT_PROFILESETTINGS_VERSION = 9;

class UUTLocalPlayer;

USTRUCT()
struct FStoredWeaponPriority
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString WeaponClassName;

	UPROPERTY()
	float WeaponPriority;

	FStoredWeaponPriority()
		: WeaponClassName(TEXT(""))
		, WeaponPriority(0.0)
	{};

	FStoredWeaponPriority(FString inWeaponClassName, float inWeaponPriority)
		: WeaponClassName(inWeaponClassName)
		, WeaponPriority(inWeaponPriority)
	{};
};

UCLASS()
class UNREALTOURNAMENT_API UUTProfileSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	void ClearWeaponPriorities();
	void SetWeaponPriority(FString WeaponClassName, float NewPriority);
	float GetWeaponPriority(FString WeaponClassName, float DefaultPriority);

	bool HasTokenBeenPickedUpBefore(FName TokenUniqueID);
	void TokenPickedUp(FName TokenUniqueID);
	void TokenRevoke(FName TokenUniqueID);
	void TokensCommit();
	void TokensReset();

	/**
	 *	Gather all of the settings so that this profile object can be saved.
	 **/
	void GatherAllSettings(UUTLocalPlayer* ProfilePlayer);

	void VersionFixup();

	/**
	 *	return the current player name.
	 **/
	FString GetPlayerName() { return PlayerName; };

	/**
	 *	Apply any settings stored in this profile object
	 **/
	void ApplyAllSettings(UUTLocalPlayer* ProfilePlayer);

	/**
	 *	Versioning
	 **/
	UPROPERTY()
	uint32 SettingsRevisionNum;

	UPROPERTY()
	FString HatPath;
	UPROPERTY()
	int32 HatVariant;
	UPROPERTY()
	FString EyewearPath;
	UPROPERTY()
	int32 EyewearVariant;
	UPROPERTY()
	FString TauntPath;
	UPROPERTY()
	FString Taunt2Path;

	UPROPERTY()
	FString CharacterPath;

	UPROPERTY()
	uint32 CountryFlag;

protected:

	/**
	 *	Profiles settings go here.  Any standard UPROPERY is supported.
	 **/

	// What is the Player name associated with this profile
	UPROPERTY()
	FString PlayerName;

	// The UInputSettings object converted in to raw data for storage.
	UPROPERTY()
	TArray<uint8> RawInputSettings;

	UPROPERTY()
	TArray<struct FInputActionKeyMapping> ActionMappings;

	UPROPERTY()
	TArray<struct FInputAxisKeyMapping> AxisMappings;

	UPROPERTY()
	TArray<struct FInputAxisConfigEntry> AxisConfig;

	UPROPERTY()
	TArray<FCustomKeyBinding> CustomBinds;

	UPROPERTY()
	uint32 bEnableMouseSmoothing:1;

	UPROPERTY()
	uint32 bEnableFOVScaling:1;

	UPROPERTY()
	uint32 bInvertMouse;

	UPROPERTY()
	float FOVScale;

	UPROPERTY()
	float DoubleClickTime;

	UPROPERTY()
	float MouseSensitivity;

	UPROPERTY()
	float MaxDodgeClickTimeValue;

	UPROPERTY()
	float MaxDodgeTapTimeValue;

	UPROPERTY()
	uint32 bSingleTapWallDodge:1;

	UPROPERTY()
	uint32 bTapCrouchToSlide : 1;

	UPROPERTY()
	uint32 bAutoSlide : 1;

	UPROPERTY()
	uint32 bSingleTapAfterJump : 1;

	UPROPERTY()
	FKey ConsoleKey;

	UPROPERTY()
	uint32 bAutoWeaponSwitch:1;

	UPROPERTY()
	float WeaponBob;

	UPROPERTY()
	float ViewBob;

	UPROPERTY()
	TEnumAsByte<EWeaponHand> WeaponHand;

	UPROPERTY()
	FLinearColor FFAPlayerColor;

	/** Holds a list of weapon class names (as string) and weapon switch priorities. - NOTE: this will only show priorities of those weapon the player has "seen" and are stored as "WeaponName:####" */
	UPROPERTY()
	TArray<FStoredWeaponPriority> WeaponPriorities;

	UPROPERTY()
	float PlayerFOV;

	// Linear list of token unique ids for serialization
	UPROPERTY()
	TArray<FName> FoundTokenUniqueIDs;

	TArray<FName> TempFoundTokenUniqueIDs;

	// If true, then the player will not show toasts in game.
	UPROPERTY()
	uint32 bSuppressToastsInGame : 1;

};