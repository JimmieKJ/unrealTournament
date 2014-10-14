// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
//
// These are settings that are stored in a remote MCP managed profile.  A copy of them are also stored in the user folder on the local machine
// in case of MCP failure or downtime.

#pragma once

#include "GameFramework/InputSettings.h"
#include "UTPlayerInput.h"
#include "UTProfileSettings.generated.h"

static const uint32 VALID_PROFILESETTINGS_VERSION = 4;
static const uint32 CURRENT_PROFILESETTINGS_VERSION = 4;

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
class UUTProfileSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	// Accessor for the simple data types.
	
	void SetPlayerName(FString NewName) 
	{ 
		if (NewName != PlayerName) 
		{
			PlayerName = NewName; 
			bDirty = true;
		}
	};

	FString GetPlayerName() { return PlayerName; };

	void SetAutoWeaponSwitch(bool bNewAutoWeaponSwitch) 
	{ 
		if (bAutoWeaponSwitch != bNewAutoWeaponSwitch) 
		{
			bAutoWeaponSwitch = bNewAutoWeaponSwitch; 
			bDirty = true; 
		}
	};

	bool GetAutoWeaponSwitch() { return bAutoWeaponSwitch; };

	void SetWeaponBob(float NewWeaponBob) 
	{ 
		if (WeaponBob != NewWeaponBob) 
		{
			WeaponBob = NewWeaponBob; 
			bDirty = true; 
		}
	};

	float GetWeaponBob() { return WeaponBob; };

	void SetViewBob(float NewViewBob) 
	{ 
		if (ViewBob != NewViewBob) 
		{
			ViewBob = NewViewBob; 
			bDirty = true; 
		}
	};

	float GetViewBob() { return ViewBob; };

	void SetFFAPlayerColor(FLinearColor NewColor)
	{
		if (FFAPlayerColor != NewColor)
		{
			FFAPlayerColor = NewColor;
			bDirty = true;
		}
	}

	FLinearColor GetFFAPlayerColor() { return FFAPlayerColor; };


	void SetWeaponPriority(FString WeaponClassName, float NewPriority);
	float GetWeaponPriority(FString WeaponClassName, float DefaultPriority);

	/**
	 *	Look at the current player and gather up the input settings for storage.
	 **/
	void GatherInputSettings();

	/**
	 *	Apply the stored input settings for the current player
	 **/
	void ApplyInputSettings();


	/**
	 *	Will return true if this profile is dirty and needs to be saved
	 **/
	bool IsDirty() { return bDirty; };

	void Dirty() { bDirty = true; };

	/**
	 *	Marks this profile as clean.
	 **/
	void Clean() { bDirty = false; };

	/**
	 *	Versioning
	 **/
	UPROPERTY()
	uint32 SettingsRevisionNum;

protected:

	/**
	 * Will be true if the settings are dirty and the profile needs to be saved.  NOTE: We do not make it a UPROPERTY because
	 * we do not want it to be stored in the payload going up to the MCP.
	 **/

	uint32 bDirty:1;

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
	FKey ConsoleKey;

	UPROPERTY()
	uint32 bAutoWeaponSwitch:1;

	UPROPERTY()
	float WeaponBob;

	UPROPERTY()
	float ViewBob;

	UPROPERTY()
	FLinearColor FFAPlayerColor;

	/** Holds a list of weapon class names (as string) and weapon switch priorities. - NOTE: this will only show priorities of those weapon the player has "seen" and are stored as "WeaponName:####" */
	UPROPERTY()
	TArray<FStoredWeaponPriority> WeaponPriorities;

};