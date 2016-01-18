// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UserWidget.h"
#include "AssetData.h"
#include "UTWeapon.h"
#include "UTWeaponReplacementUserWidget.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTWeaponReplacementUserWidget : public UUserWidget
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
