// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
#include "UTWeap_Enforcer.h"
#include "UTCTFGameMode.h"
#include "UTCarriedObject.h"
#include "UTCharacterContent.h"
#include "UTImpactEffect.h"

#if WITH_PROFILE
#include "OnlineSubsystemMcp.h"
#include "GameServiceMcp.h"
#endif

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
void UUTCheatManager::Spread(float Scaling)
{
	AUTCharacter* MyPawn = Cast<AUTCharacter>(GetOuterAPlayerController()->GetPawn());
	if (!MyPawn)
	{
		return;
	}
	for (TInventoryIterator<AUTWeapon> It(MyPawn); It; ++It)
	{
		for (int32 i = 0; i < It->Spread.Num(); i++)
		{
			It->Spread[i] = It->GetClass()->GetDefaultObject<AUTWeapon>()->Spread[i] * Scaling;
			if (It->Spread[i] != 0.f)
			{
				UE_LOG(UT, Warning, TEXT("%s New Spread %d is %f"), *It->GetName(), i, It->Spread[i]);
			}
		}
	}

}

void UUTCheatManager::Sum()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(GetOuterAPlayerController()->Player);
	AUTGameState* GS = GetOuterAPlayerController()->GetWorld()->GetGameState<AUTGameState>();
	if (LP && GS)
	{
		LP->OpenMatchSummary(GS);
	}
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
		if (MyPawn->CharacterData.GetDefaultObject()->GibExplosionEffect != NULL)
		{
			MyPawn->CharacterData.GetDefaultObject()->GibExplosionEffect.GetDefaultObject()->SpawnEffect(MyPawn->GetWorld(), MyPawn->GetRootComponent()->GetComponentTransform(), MyPawn->GetMesh(), MyPawn, NULL, SRT_None);
		}
		for (const FGibSlotInfo& GibInfo : MyPawn->CharacterData.GetDefaultObject()->Gibs)
		{
			MyPawn->SpawnGib(GibInfo, *MyPawn->LastTakeHitInfo.DamageType);
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
		AUTWeap_Enforcer* Enforcer = Cast<AUTWeap_Enforcer>(MyPawn->FindInventoryType(TSubclassOf<AUTInventory>(AUTWeap_Enforcer::StaticClass()), false));
		if (Enforcer)
		{
			Enforcer->BecomeDual();
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

void UUTCheatManager::ImpartWeaponSkin(const FString& Skin)
{
	FString SkinPackageName;
	if (FPackageName::SearchForPackageOnDisk(Skin, &SkinPackageName))
	{
		AUTGameState* GS = GetOuterAPlayerController()->GetWorld()->GetGameState<AUTGameState>();
		for (int32 i = 0; i < GS->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
			if (PS)
			{
				PS->ServerReceiveWeaponSkin(SkinPackageName);
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

void UUTCheatManager::Teleport()
{
	AUTCharacter* UTChar = Cast<AUTCharacter>(GetOuterAPlayerController()->GetPawn());
	if (UTChar != NULL)
	{
		FHitResult Hit(1.f);
		ECollisionChannel TraceChannel = COLLISION_TRACE_WEAPONNOCHARACTER;
		FCollisionQueryParams QueryParams(GetClass()->GetFName(), true, UTChar);
		FVector StartLocation(0.f);
		FRotator SpawnRotation(0.f);
		UTChar->GetActorEyesViewPoint(StartLocation, SpawnRotation);
		const FVector EndTrace = StartLocation + SpawnRotation.Vector() * 20000.f;

		if (!GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndTrace, TraceChannel, QueryParams))
		{
			Hit.Location = EndTrace;
		}
		UTChar->TeleportTo(Hit.Location, SpawnRotation);
	}
}

void UUTCheatManager::McpGrantItem(const FString& ItemId)
{
	/*
	if (GetOuterAUTPlayerController()->McpProfile)
	{
		TArray<FString> ItemList;
		ItemList.Push(ItemId);
		FDevCheatUrlContext Context(FMcpQueryComplete::CreateUObject(this, &UUTCheatManager::LogWebResponse));
		GetOuterAUTPlayerController()->McpProfile->GrantItems(ItemList, 1, Context);
	}*/
}

void UUTCheatManager::McpDestroyItem(const FString& ItemId)
{
	/*
	if (GetOuterAUTPlayerController()->McpProfile)
	{
		FDevCheatUrlContext Context(FMcpQueryComplete::CreateUObject(this, &UUTCheatManager::LogWebResponse));
		GetOuterAUTPlayerController()->McpProfile->DestroyItems(ItemId, 1, Context);
	}*/
}

#if WITH_PROFILE

void UUTCheatManager::LogWebResponse(const FMcpQueryResult& Response)
{
	/*
	if (!Response.bSucceeded)
	{
		GetOuterAPlayerController()->ClientMessage(TEXT("Cheat failed"));
		UE_LOG(UT, Warning, TEXT("%s"), *Response.ErrorMessage.ToString());
	}
	else
	{
		GetOuterAPlayerController()->ClientMessage(TEXT("Cheat succeeded"));
	}*/
}

#endif

void UUTCheatManager::McpCheat()
{
#if WITH_PROFILE
	AUTPlayerController* const MyPC = GetOuterAUTPlayerController();
	UUtMcpProfile* McpProfile = MyPC->GetMcpProfile();
	if (McpProfile)
	{
		FOnlineSubsystemMcp* OnlineSubMcp = McpProfile->GetOnlineSubMcp();
		if (OnlineSubMcp && OnlineSubMcp->GetMcpGameService().IsValid())
		{
			FString ServiceUrl = OnlineSubMcp->GetMcpGameService()->GetBaseUrl();

			// this is a bit of a hack, but it's for a cheat...
			ServiceUrl.ReplaceInline(TEXT("-public-service"), TEXT("-admin-service"));
			ServiceUrl.ReplaceInline(TEXT(".epicgames.com/"), TEXT(".epicgames.net/"));
			ServiceUrl += TEXT("/admin#");
			ServiceUrl += McpProfile->GetProfileGroup().GetGameAccountId().ToString();

			// open the url in the browser
			FPlatformProcess::LaunchURL(*ServiceUrl, nullptr, nullptr);
		}
	}
#endif
}

void UUTCheatManager::McpRefreshProfile()
{
#if WITH_PROFILE
	AUTPlayerController* const MyPC = GetOuterAUTPlayerController();
	UUtMcpProfile* McpProfile = MyPC->GetMcpProfile();
	if (McpProfile)
	{
		McpProfile->ForceQueryProfile(FMcpQueryComplete());
	}
#endif
}

void UUTCheatManager::SoloQueueMe(int32 PlaylistId)
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(GetOuterAPlayerController()->Player);
	if (LP)
	{
		LP->StartSoloQueueMatchmaking(PlaylistId);
	}
}