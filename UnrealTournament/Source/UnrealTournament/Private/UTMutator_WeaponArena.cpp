// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"

#include "UTMutator_WeaponArena.h"
#include "UTPickupWeapon.h"
#include "UTPickupAmmo.h"
#include "UTGameMode.h"
#include "AssetData.h"
#include "UTWeap_Translocator.h"

#if !UE_SERVER
#include "UserWidget.h"
#endif

AUTMutator_WeaponArena::AUTMutator_WeaponArena(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = NSLOCTEXT("Mutator_WeaponArena", "Display Name", "Weapon Arena");
	GroupNames.Add(FName(TEXT("Arena")));
	ArenaWeaponPath = "/Game/RestrictedAssets/Weapons/RocketLauncher/BP_RocketLauncher.BP_RocketLauncher_C"; // warning: soft ref
	bAllowTranslocator = true;
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UClass> ConfigMenuClass(TEXT("/Game/RestrictedAssets/Blueprints/ArenaMutatorMenu.ArenaMutatorMenu_C"));
		ConfigMenu = ConfigMenuClass.Object;
	}
#endif
}

void AUTMutator_WeaponArena::BeginPlay()
{
	Super::BeginPlay();

	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode != NULL && ArenaWeaponType != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			GS->SpawnProtectionTime = 0.5f;
		}
		for (int32 i = GameMode->DefaultInventory.Num() - 1; i >= 0; i--)
		{
			if (GameMode->DefaultInventory[i] != ArenaWeaponType && (!bAllowTranslocator || GameMode->DefaultInventory[i] == NULL || !GameMode->DefaultInventory[i]->IsChildOf(AUTWeap_Translocator::StaticClass())))
			{
				GameMode->DefaultInventory.RemoveAt(i);
			}
		}
		GameMode->DefaultInventory.AddUnique(ArenaWeaponType);
	}
}

void AUTMutator_WeaponArena::ModifyPlayer_Implementation(APawn* Other, bool bIsNewSpawn)
{
	if (bIsNewSpawn)
	{
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(Other);
		for (int32 i = UTCharacter->DefaultCharacterInventory.Num() - 1; i >= 0; i--)
		{
			if (!bAllowTranslocator || UTCharacter->DefaultCharacterInventory[i] == NULL || !UTCharacter->DefaultCharacterInventory[i]->IsChildOf(AUTWeap_Translocator::StaticClass()))
			{
				UTCharacter->DefaultCharacterInventory.RemoveAt(i);
			}
		}
	}
	return Super::ModifyPlayer_Implementation(Other, bIsNewSpawn);
}

bool AUTMutator_WeaponArena::CheckRelevance_Implementation(AActor* Other)
{
	// recursion guard in case some mutator also tries to replace ammo so we don't end up in infinite recursion
	static bool bRecursionGuard = false;
	if (bRecursionGuard)
	{
		return Super::CheckRelevance_Implementation(Other);
	}
	else if (Cast<AUTWeapon>(Other) != NULL)
	{
		if (Other->GetClass() != ArenaWeaponType && (!bAllowTranslocator || !Other->IsA(AUTWeap_Translocator::StaticClass())))
		{
			return false;
		}
		else
		{
			// modify AI rating so bots realize they're not going to get anything better
			((AUTWeapon*)Other)->BaseAISelectRating = FMath::Max<float>(0.6f, ((AUTWeapon*)Other)->BaseAISelectRating);
			return Super::CheckRelevance_Implementation(Other);
		}
	}
	else
	{
		TGuardValue<bool> Guard(bRecursionGuard, true);
		AUTPickupWeapon* WeaponPickup = Cast<AUTPickupWeapon>(Other);
		if (WeaponPickup != NULL)
		{
			WeaponPickup->SetInventoryType(ArenaWeaponType);
			return Super::CheckRelevance_Implementation(Other);
		}
		else
		{
			AUTPickupAmmo* AmmoPickup = Cast<AUTPickupAmmo>(Other);
			if (AmmoPickup != NULL && AmmoPickup->Ammo.Type != ArenaAmmoType)
			{
				if (ArenaAmmoType != NULL)
				{
					GetWorld()->SpawnActor<AUTPickupAmmo>(ArenaAmmoType, AmmoPickup->GetActorLocation(), AmmoPickup->GetActorRotation());
				}
				return false;
			}
			else
			{
				return Super::CheckRelevance_Implementation(Other);
			}
		}
	}
}

void AUTMutator_WeaponArena::GetGameURLOptions_Implementation(TArray<FString>& OptionsList) const
{
	OptionsList.Add(FString::Printf(TEXT("ArenaWeaponPath=%s"), *ArenaWeaponPath));
}

void AUTMutator_WeaponArena::Init_Implementation(const FString& Options)
{
	FString NewWeaponPath = ParseOption(Options, TEXT("ArenaWeaponPath"));	
	if (!NewWeaponPath.IsEmpty())
	{
		ArenaWeaponPath = NewWeaponPath;
	}

	// cache weapon and ammo classes
	ArenaWeaponType = LoadClass<AUTWeapon>(NULL, *ArenaWeaponPath, NULL, LOAD_None, NULL);
	if (ArenaWeaponType != NULL)
	{
		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTPickupAmmo::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_Ammo(TEXT("Ammo"));
			const FString* AmmoData = Asset.TagsAndValues.Find(NAME_Ammo);
			if (AmmoData != NULL && AmmoData->Contains(ArenaWeaponType->GetPathName()))
			{
				static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
				const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
				if (ClassPath != NULL)
				{
					ArenaAmmoType = LoadClass<AUTPickupAmmo>(NULL, **ClassPath, NULL, LOAD_None, NULL);
					if (ArenaAmmoType != NULL)
					{
						break;
					}
				}
			}
		}
	}
}
