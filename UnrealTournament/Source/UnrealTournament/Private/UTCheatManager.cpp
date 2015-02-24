// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
#include "UTGameMode.h"

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
	AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (Game != NULL)
	{
		Game->bAmmoIsLimited = false;
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

void UUTCheatManager::ImpartHats(const FString& Hat)
{
	AUTGameState* GS = GetOuterAPlayerController()->GetWorld()->GetGameState<AUTGameState>();
	for (int32 i = 0; i < GS->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
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
}

void UUTCheatManager::SetEyewear(const FString& Eyewear)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(GetOuterAPlayerController()->PlayerState);
	if (PS)
	{
		FString EyewearPackageName;
		if (FPackageName::SearchForPackageOnDisk(Eyewear, &EyewearPackageName))
		{
			EyewearPackageName += TEXT(".") + Eyewear + TEXT("_C");
			PS->ServerReceiveEyewearClass(EyewearPackageName);
		}
	}
}

void UUTCheatManager::ImpartEyewear(const FString& Eyewear)
{
	AUTGameState* GS = GetOuterAPlayerController()->GetWorld()->GetGameState<AUTGameState>();
	for (int32 i = 0; i < GS->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
		if (PS)
		{
			FString EyewearPackageName;
			if (FPackageName::SearchForPackageOnDisk(Eyewear, &EyewearPackageName))
			{
				EyewearPackageName += TEXT(".") + Eyewear + TEXT("_C");
				PS->ServerReceiveEyewearClass(EyewearPackageName);
			}
		}
	}
}