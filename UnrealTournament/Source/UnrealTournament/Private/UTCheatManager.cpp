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
#include "UTSpreeMessage.h"


UUTCheatManager::UUTCheatManager(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUTCheatManager::Ann(int32 Switch)
{
	// play an announcement for testing
	GetOuterAPlayerController()->ClientReceiveLocalizedMessage(UUTSpreeMessage::StaticClass(), Switch, GetOuterAPlayerController()->PlayerState, GetOuterAPlayerController()->PlayerState, NULL);
}

void UUTCheatManager::AllAmmo()
{
	AUTCharacter* MyPawn = Cast<AUTCharacter>(GetOuterAPlayerController()->GetPawn());
	if (MyPawn != NULL)
	{
		MyPawn->AllAmmo();
	}
}

void UUTCheatManager::UnlimitedAmmo()
{
	AUTCharacter* MyPawn = Cast<AUTCharacter>(GetOuterAPlayerController()->GetPawn());
	if (MyPawn != NULL)
	{
		MyPawn->UnlimitedAmmo();
	}
}

void UUTCheatManager::Loaded()
{
	AUTCharacter* MyPawn = Cast<AUTCharacter>(GetOuterAPlayerController()->GetPawn());
	if (MyPawn != NULL)
	{
		// grant all weapons that are in memory
		for (TObjectIterator<UClass> It; It; ++It)
		{
			// make sure we don't use abstract, deprecated, or blueprint skeleton classes
			if (It->IsChildOf(AUTWeapon::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) && !It->GetName().StartsWith(TEXT("SKEL_")))
			{
				UClass* TestClass = *It;
				if (!MyPawn->FindInventoryType(TSubclassOf<AUTInventory>(*It), true))
				{
					MyPawn->AddInventory(MyPawn->GetWorld()->SpawnActor<AUTInventory>(*It, FVector(0.0f), FRotator(0, 0, 0)), true);
				}
			}
		}
		MyPawn->AllAmmo();
	}
}

void UUTCheatManager::ua()
{
	UnlimitedAmmo();
}

void UUTCheatManager::SetHat(const FString& Hat)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(GetOuterAPlayerController()->PlayerState);
	if (PS)
	{
		FString HatPackageName;
		if (FPackageName::SearchForPackageOnDisk(Hat, &HatPackageName))
		{
			HatPackageName += TEXT(".") + Hat + TEXT("_C");
			PS->ServerReceiveHatClass(HatPackageName);
		}
	}
}