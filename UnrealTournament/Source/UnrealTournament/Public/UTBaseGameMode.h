// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameMode.h"
#include "../Private/Slate/SUWindowsMidGame.h"
#include "UTBaseGameMode.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTBaseGameMode : public AGameMode
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(GlobalConfig)
	FString ServerPassword;

	uint32 bRequirePassword:1;

#if !UE_SERVER

	/**
	 *	Returns the Menu to popup when the user requests a menu
	 **/
	virtual TSharedRef<SUWindowsDesktop> GetGameMenu(UUTLocalPlayer* PlayerOwner) const
	{
		return SNew(SUWindowsMidGame).PlayerOwner(PlayerOwner);
	}

#endif

protected:

	// Will be > 0 if this is an instance created by lobby
	uint32 LobbyInstanceID;

public:

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// The Unique ID for this game instance.
	FGuid ServerInstanceGUID;

	virtual bool IsGameInstanceServer() { return LobbyInstanceID > 0; }
	virtual bool IsLobbyServer() { return false; }



};