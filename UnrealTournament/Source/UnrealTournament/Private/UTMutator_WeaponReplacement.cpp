// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"

#include "UTMutator_WeaponReplacement.h"
#include "UTPickupWeapon.h"
#include "UTPickupAmmo.h"
#include "UTGameMode.h"
#include "AssetData.h"

#if !UE_SERVER
#include "UserWidget.h"
#endif

AUTMutator_WeaponReplacement::AUTMutator_WeaponReplacement(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DisplayName = NSLOCTEXT("Mutator_WeaponReplacement", "Display Name", "Weapon Replacement");
	GroupNames.Add(FName(TEXT("Arena")));
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UClass> ConfigMenuClass(TEXT("/Game/RestrictedAssets/Blueprints/Mutator_WeaponReplacementUI.Mutator_WeaponReplacementUI_C"));
		ConfigMenu = ConfigMenuClass.Object;
	}
#endif
}

void AUTMutator_WeaponReplacement::BeginPlay()
{
	Super::BeginPlay();

	AUTGameMode* GameMode = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());
	if (GameMode != NULL)
	{
		for (int32 i=0; i<GameMode->DefaultInventory.Num(); i++)
		{
			for (int32 j=0; j < WeaponsToReplace.Num(); j++)
			{
				FReplacementInfo info = WeaponsToReplace[j];
				UClass* oldWeapon = StaticLoadClass(AUTWeapon::StaticClass(), nullptr, *info.OldClassPath);
				if (GameMode->DefaultInventory[i] == oldWeapon)
				{
					GameMode->DefaultInventory[i] = StaticLoadClass(AUTWeapon::StaticClass(), nullptr, *info.NewClassPath);
				}
			}
		}
	}
}

void AUTMutator_WeaponReplacement::ModifyPlayer_Implementation(APawn* Other, bool bIsNewSpawn)
{
	if (bIsNewSpawn)
	{
		AUTCharacter* UTCharacter = Cast<AUTCharacter>(Other);
		for (int32 i = 0; i < UTCharacter->DefaultCharacterInventory.Num(); i++)
		{
			for (int32 j = 0; j < WeaponsToReplace.Num(); j++)
			{
				FReplacementInfo info = WeaponsToReplace[j];
				UClass* oldWeapon = StaticLoadClass(AUTWeapon::StaticClass(), nullptr, *info.OldClassPath);
				if (UTCharacter->DefaultCharacterInventory[i] == oldWeapon)
				{
					UTCharacter->DefaultCharacterInventory[i] = StaticLoadClass(AUTWeapon::StaticClass(), nullptr, *info.NewClassPath);
				}
			}
		}
	}
	return Super::ModifyPlayer_Implementation(Other, bIsNewSpawn);
}

bool AUTMutator_WeaponReplacement::CheckRelevance_Implementation(AActor* Other)
{
	// prevent recursion; this prevents sequential replacements from screwing things up
	// (i.e. if user says Shock -> Rocket and then Rocket -> Minigun, make sure we don't cause Shock ammo to recurse and end up as Minigun ammo)
	static bool bRecursionGuard = false;
	if (bRecursionGuard)
	{
		return Super::CheckRelevance_Implementation(Other);
	}
	TGuardValue<bool> Guard(bRecursionGuard, true);

	AUTPickupWeapon* WeaponPickup = Cast<AUTPickupWeapon>(Other);
	if (WeaponPickup != NULL)
	{
		for (int32 i = 0; i < WeaponsToReplace.Num(); i++)
		{
			const FReplacementInfo& Info = WeaponsToReplace[i];
			UClass* OldWeapon = StaticLoadClass(AUTWeapon::StaticClass(), nullptr, *Info.OldClassPath);
			if (WeaponPickup->WeaponType == OldWeapon)
			{
				WeaponPickup->WeaponType = StaticLoadClass(AUTWeapon::StaticClass(), nullptr, *Info.NewClassPath);
				break;
			}
		}
	}
	else
	{
		AUTPickupAmmo* AmmoPickup = Cast<AUTPickupAmmo>(Other);
		if (AmmoPickup != NULL)
		{
			for (int32 i = 0; i < WeaponsToReplace.Num(); i++)
			{
				const FReplacementInfo& Info = WeaponsToReplace[i];
				UClass* OldWeapon = StaticLoadClass(AUTWeapon::StaticClass(), nullptr, *Info.OldClassPath);
				if (Info.OldClassPath != Info.NewClassPath && AmmoPickup->Ammo.Type == OldWeapon)
				{
					UClass* NewWeapon = StaticLoadClass(AUTWeapon::StaticClass(), nullptr, *Info.NewClassPath);
					TArray<FAssetData> AssetList;
					GetAllBlueprintAssetData(AUTPickupAmmo::StaticClass(), AssetList);
					for (const FAssetData& Asset : AssetList)
					{
						static FName NAME_Ammo(TEXT("Ammo"));
						const FString* AmmoData = Asset.TagsAndValues.Find(NAME_Ammo);
						if (AmmoData != NULL && AmmoData->Contains(NewWeapon->GetPathName()))
						{
							static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
							const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
							if (ClassPath != NULL)
							{
								UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
								AUTPickupAmmo* A = Cast<AUTPickupAmmo>(TestClass->GetDefaultObject());
								if (A != NULL && A->Ammo.Type == NewWeapon)
								{
									FVector AmmoLocation = AmmoPickup->GetActorLocation();
									FRotator AmmoRotation = AmmoPickup->GetActorRotation();
									GetWorld()->SpawnActor(TestClass, &AmmoLocation, &AmmoRotation);
									return false;
								}
							}
						}
					}
				}
			}
		}
	}

	return Super::CheckRelevance_Implementation(Other);
}

void AUTMutator_WeaponReplacement::Init_Implementation(const FString& Options)
{
	FString WeaponList = ParseOption(Options, TEXT("WTR"));
	if (!WeaponList.IsEmpty())
	{
		WeaponsToReplace.Empty();
		TArray<FString> WeaponInfos;
		WeaponList.ParseIntoArray(WeaponInfos, TEXT(","), true);
		for (int32 i=0; i< WeaponInfos.Num(); i++)
		{
			TArray<FString> WeaponInfo;
			WeaponInfos[i].ParseIntoArray(WeaponInfo, TEXT(":"), true);
			if (WeaponInfo.Num() == 2)
			{
				WeaponsToReplace.Add(FReplacementInfo(WeaponInfo[0],WeaponInfo[1]));
			}
		}
	}
}

void AUTMutator_WeaponReplacement::GetGameURLOptions_Implementation(TArray<FString>& OptionsList)
{
	FString Replacements;

	for (int32 i=0; i < WeaponsToReplace.Num(); i++)
	{
		if (Replacements.IsEmpty())
		{
			Replacements += FString::Printf(TEXT("%s:%s"), *WeaponsToReplace[i].OldClassPath, *WeaponsToReplace[i].NewClassPath);
		}
		else
		{
			Replacements += FString::Printf(TEXT(",%s:%s"), *WeaponsToReplace[i].OldClassPath, *WeaponsToReplace[i].NewClassPath);
		}
	}

	if (!Replacements.IsEmpty())
	{
		OptionsList.Add(FString::Printf(TEXT("WTR=%s"), *Replacements));
	}
}
