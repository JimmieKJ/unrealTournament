// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponReplacementUserWidget.generated.h"

#if !UE_SERVER

#include "UserWidget.h"

#include "AssetData.h"
#include "UTWeapon.h"

UCLASS()
class UUTWeaponReplacementUserWidget : public UUserWidget
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<AUTWeapon*> WeaponList;

	UFUNCTION(BlueprintCallable, Category="UI Runnables")
	void BuildWeaponList();

	/** get a unique display name for the weapon, working around string combo box limitations */
	UFUNCTION(BlueprintCallable, Category = "UI Runnables")
	FString GetUniqueDisplayName(const AUTWeapon* Weapon = NULL, const FString& PathName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "UI Runnables")
	FString DisplayNameToPathName(const FString& DisplayName);
};

#endif