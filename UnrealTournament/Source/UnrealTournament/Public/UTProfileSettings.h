// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
//
// These are settings that are stored in a remote MCP managed profile.  A copy of them are also stored in the user folder on the local machine
// in case of MCP failure or downtime.

#pragma once

#include "GameFramework/InputSettings.h"
#include "UTPlayerInput.h"
#include "UTProfileSettings.generated.h"

static const uint32 VALID_PROFILESETTINGS_VERSION = 2;
static const uint32 CURRENT_PROFILESETTINGS_VERSION = 2;

UCLASS()
class UUTProfileSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	void SetPlayerName(FString NewName);
	FString GetPlayerName();

	void GatherInputSettings();
	void ApplyInputSettings();

	UPROPERTY()
	uint32 SettingsRevisionNum;

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
	FKey ConsoleKey;
};