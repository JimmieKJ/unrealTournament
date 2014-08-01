// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUD.h"
#include "UTLocalPlayer.h"
#include "UTPlayerState.h"
#include "UTPlayerController.h"
#include "UTCharacterMovement.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "UTPickup.h"
#include "UTPickupInventory.h"
#include "UTPickupWeapon.h"
#include "UTAnnouncer.h"
#include "UTHUDWidgetMessage.h"
#include "UTPlayerInput.h"
#include "UTPlayerCameraManager.h"
#include "UTCheatManager.h"


UUTCheatManager::UUTCheatManager(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UUTCheatManager::AllAmmo()
{
	AUTCharacter* MyPawn = Cast<AUTCharacter>(GetOuterAPlayerController()->GetPawn());
	if (MyPawn)
	{
		MyPawn->AllAmmo();
	}
}

void UUTCheatManager::UnlimitedAmmo()
{
	AUTCharacter* MyPawn = Cast<AUTCharacter>(GetOuterAPlayerController()->GetPawn());
	if (MyPawn)
	{
		MyPawn->UnlimitedAmmo();
	}
}

void UUTCheatManager::ua()
{
	UnlimitedAmmo();
}