// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
//
// These are settings that are stored in a remote MCP managed profile.  A copy of them are also stored in the user folder on the local machine
// in case of MCP failure or downtime.

#pragma once

#include "UTProfileSettings.generated.h"

UCLASS()
class UUTProfileSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	void SetPlayerName(FString NewName);
	FString GetPlayerName();

protected:

	/**
	 *	Profiles settings go here.  Any standard UPROPERY is supported.
	 **/

	// What is the Player name associated with this profile
	UPROPERTY()
	FString PlayerName;

};