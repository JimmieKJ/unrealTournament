// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameMode.h"
#include "../Private/Slate/SUWindowsMidGame.h"
#include "UTBaseGameMode.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTBaseGameMode : public AGameMode
{
	GENERATED_UCLASS_BODY()

#if !UE_SERVER

	/**
	 *	Returns the Menu to popup when the user requests a menu
	 **/
	virtual TSharedRef<SUWindowsDesktop> GetGameMenu(UUTLocalPlayer* PlayerOwner) const
	{
		return SNew(SUWindowsMidGame).PlayerOwner(PlayerOwner);
	}

#endif



};