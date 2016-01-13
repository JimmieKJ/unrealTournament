// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"

#include "UTMutator_WeaponReplacement.h"
#include "UTWeapon.h"
#include "UTPickupAmmo.h"
#include "UTGameMode.h"
#include "AssetData.h"

#include "UTWeaponReplacementUserWidget.h"

UUTWeaponReplacementUserWidget::UUTWeaponReplacementUserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUTWeaponReplacementUserWidget::BuildWeaponList()
{
	if (WeaponList.Num() > 0)
	{
		return;
	}
	TArray<FAssetData> AssetList;
	GetAllBlueprintAssetData(AUTWeapon::StaticClass(), AssetList);
	for (const FAssetData& Asset : AssetList)
	{
		static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
		const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
		if (ClassPath != NULL)
		{
			UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
			if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTWeapon::StaticClass()) && !TestClass->GetDefaultObject<AUTWeapon>()->bHideInMenus)
			{
				WeaponList.Add(TestClass->GetDefaultObject<AUTWeapon>());
			}
		}
	}
}

FString UUTWeaponReplacementUserWidget::GetUniqueDisplayName(const AUTWeapon* Weapon, const FString& PathName)
{
	if (Weapon == NULL && PathName.Len() > 0)
	{
		UClass* WeaponClass = LoadClass<AUTWeapon>(NULL, *PathName, NULL, LOAD_None, NULL);
		if (WeaponClass != NULL)
		{
			Weapon = WeaponClass->GetDefaultObject<AUTWeapon>();
		}
	}
	if (Weapon == NULL)
	{
		return FString(TEXT("None"));
	}
	else
	{
		FString DisplayName = Weapon->DisplayName.ToString();

		FString PackageName = Weapon->GetOutermost()->GetName();
		if (PackageName.StartsWith(TEXT("/Game/")))
		{
			// special postfix for debug weapons
			if (PackageName.Contains(TEXT("EpicInternal")))
			{
				DisplayName += TEXT(" (Epic Internal)");
			}
		}
		else
		{
			// postfix plugin name if not in core game, so plugins don't have to worry about name conflicts with anything but themselves
			TArray<FString> Pieces;
			PackageName.ParseIntoArray(Pieces, TEXT("/"), true);
			DisplayName += TEXT(" (") + Pieces[0] + TEXT(")");
		}
		return DisplayName;
	}
}

FString UUTWeaponReplacementUserWidget::DisplayNameToPathName(const FString& DisplayName)
{
	if (WeaponList.Num() == 0)
	{
		BuildWeaponList();
	}
	for (AUTWeapon* Weapon : WeaponList)
	{
		if (GetUniqueDisplayName(Weapon) == DisplayName)
		{
			return Weapon->GetClass()->GetPathName();
		}
	}
	return TEXT("None");
}
