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
#include "UTCTFGameMessage.h"
#include "UTCTFRewardMessage.h"
#include "UTCountDownMessage.h"
#include "UTDeathMessage.h"
#include "UTPickupMessage.h"
#include "UTMultiKillMessage.h"
#include "UTGameMode.h"
#include "UTWeap_Translocator.h"
#include "UTCTFGameMode.h"
#include "UTCarriedObject.h"
#include "UTCharacterContent.h"
#include "UTImpactEffect.h"

UUTCheatManager::UUTCheatManager(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUTCheatManager::Ann(int32 Switch)
{
	// play an announcement for testing
/*	AUTCTFGameMode* CTF = GetWorld()->GetAuthGameMode<AUTCTFGameMode>();
	AUTCarriedObject* Flag = CTF->CTFGameState->FlagBases[0]->GetCarriedObject();
	AUTPlayerState* Holder = Cast<AUTPlayerState>(GetOuterAPlayerController()->PlayerState);
	Flag->SendGameMessage(4, Holder, NULL);
	Flag->SendGameMessage(3, Holder, NULL);
	Flag->SendGameMessage(1, NULL, NULL);
	Flag->SendGameMessage(4, Holder, NULL);
	CTF->BroadcastScoreUpdate(Holder, Holder->Team);
	Flag->SendGameMessage(4, Holder, NULL);
	Flag->SendGameMessage(3, Holder, NULL);
	Flag->SendGameMessage(1, NULL, NULL);
	Flag->SendGameMessage(4, Holder, NULL);
	CTF->BroadcastScoreUpdate(Holder, Holder->Team);
	Flag->SendGameMessage(4, Holder, NULL);
	Flag->SendGameMessage(3, Holder, NULL);
	Flag->SendGameMessage(4, Holder, NULL);
	Flag->SendGameMessage(3, Holder, NULL);
	Flag->SendGameMessage(4, Holder, NULL);
	Flag->SendGameMessage(3, Holder, NULL);
	Flag->SendGameMessage(4, Holder, NULL);
	Flag->SendGameMessage(3, Holder, NULL);
	Flag->SendGameMessage(4, Holder, NULL);
	Flag->SendGameMessage(3, Holder, NULL);
	Flag->SendGameMessage(1, NULL, NULL);
	Flag->SendGameMessage(4, Holder, NULL);
	Flag->SendGameMessage(3, Holder, NULL);
	Flag->SendGameMessage(1, NULL, NULL);
	Flag->SendGameMessage(4, Holder, NULL);
	Flag->SendGameMessage(3, Holder, NULL);
	Flag->SendGameMessage(1, NULL, NULL);*/

	GetOuterAPlayerController()->ClientReceiveLocalizedMessage(UUTSpreeMessage::StaticClass(), Switch, GetOuterAPlayerController()->PlayerState, GetOuterAPlayerController()->PlayerState, NULL);
//	GetOuterAPlayerController()->ClientReceiveLocalizedMessage(UUTCountDownMessage::StaticClass(), Switch, GetOuterAPlayerController()->PlayerState, GetOuterAPlayerController()->PlayerState, NULL);
//	GetOuterAPlayerController()->ClientReceiveLocalizedMessage(UUTDeathMessage::StaticClass(), Switch, GetOuterAPlayerController()->PlayerState, GetOuterAPlayerController()->PlayerState, NULL);
//	GetOuterAPlayerController()->ClientReceiveLocalizedMessage(UUTPickupMessage::StaticClass(), Switch, GetOuterAPlayerController()->PlayerState, GetOuterAPlayerController()->PlayerState, NULL);
//	GetOuterAPlayerController()->ClientReceiveLocalizedMessage(UUTMultiKillMessage::StaticClass(), Switch, GetOuterAPlayerController()->PlayerState, GetOuterAPlayerController()->PlayerState, NULL);
}

void UUTCheatManager::AllAmmo()
{
	AUTCharacter* MyPawn = Cast<AUTCharacter>(GetOuterAPlayerController()->GetPawn());
	if (MyPawn != NULL)
	{
		MyPawn->AllAmmo();
	}
}

void UUTCheatManager::Gibs()
{
	AUTCharacter* MyPawn = Cast<AUTCharacter>(GetOuterAPlayerController()->GetPawn());
	if (MyPawn != NULL)
	{
		if (MyPawn->GibExplosionEffect != NULL)
		{
			MyPawn->GibExplosionEffect.GetDefaultObject()->SpawnEffect(MyPawn->GetWorld(), MyPawn->GetRootComponent()->GetComponentTransform(), MyPawn->GetMesh(), MyPawn, NULL, SRT_None);
		}
		for (FName BoneName : MyPawn->GibExplosionBones)
		{
			MyPawn->SpawnGib(BoneName, *MyPawn->LastTakeHitInfo.DamageType);
		}
		MyPawn->TeleportTo(MyPawn->GetActorLocation() - 800.f * MyPawn->GetActorRotation().Vector(), MyPawn->GetActorRotation(), true);
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
			if (It->IsChildOf(AUTWeapon::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) && !It->GetName().StartsWith(TEXT("SKEL_")) && !It->IsChildOf(AUTWeap_Translocator::StaticClass()))
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

void UUTCheatManager::BugItWorker(FVector TheLocation, FRotator TheRotation)
{
	Super::BugItWorker(TheLocation, TheRotation);

	// The teleport doesn't play nice with our physics, so just go to walk mode rather than fall through the world
	Walk();
}

void UUTCheatManager::SetChar(const FString& NewChar)
{
	AUTGameState* GS = GetOuterAPlayerController()->GetWorld()->GetGameState<AUTGameState>();
	for (int32 i = 0; i < GS->PlayerArray.Num(); i++)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
		if (PS)
		{
			bool bFoundCharacterContent = false;
			FString NewCharPackageName;
			if (FPackageName::SearchForPackageOnDisk(NewChar, &NewCharPackageName))
			{
				NewCharPackageName += TEXT(".") + NewChar + TEXT("_C");
				if (LoadClass<AUTCharacterContent>(NULL, *NewCharPackageName, NULL, LOAD_None, NULL))
				{
					bFoundCharacterContent = true;
				}
			}

			FString NewCharAlt = NewChar + TEXT("CharacterContent");
			if (!bFoundCharacterContent && FPackageName::SearchForPackageOnDisk(NewCharAlt, &NewCharPackageName))
			{
				NewCharPackageName += TEXT(".") + NewCharAlt + TEXT("_C");
				if (LoadClass<AUTCharacterContent>(NULL, *NewCharPackageName, NULL, LOAD_None, NULL))
				{
					bFoundCharacterContent = true;
				}
			}

			FString NewCharAlt2 = NewChar + TEXT("Character");
			if (!bFoundCharacterContent && FPackageName::SearchForPackageOnDisk(NewCharAlt2, &NewCharPackageName))
			{
				NewCharPackageName += TEXT(".") + NewCharAlt2 + TEXT("_C");
				if (LoadClass<AUTCharacterContent>(NULL, *NewCharPackageName, NULL, LOAD_None, NULL))
				{
					bFoundCharacterContent = true;
				}
			}

			if (bFoundCharacterContent)
			{
				PS->ServerSetCharacter(NewCharPackageName);
			}
		}
	}
}

void UUTCheatManager::God()
{
	AUTCharacter* UTChar = Cast<AUTCharacter>(GetOuterAPlayerController()->GetPawn());
	if (UTChar != NULL)
	{
		if (UTChar->bDamageHurtsHealth)
		{
			UTChar->bDamageHurtsHealth = false;
			GetOuterAPlayerController()->ClientMessage(TEXT("God mode on"));
		}
		else
		{
			UTChar->bDamageHurtsHealth = true;
			GetOuterAPlayerController()->ClientMessage(TEXT("God Mode off"));
		}
	}
	else
	{
		Super::God();
	}
}
