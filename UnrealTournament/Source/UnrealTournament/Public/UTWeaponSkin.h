// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponSkin.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTWeaponSkin : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, AssetRegistrySearchable)
	FText DisplayName;
	
	UPROPERTY(EditAnywhere)
	FString ItemAuthor;

	/** if set a UTProfileItem is required for this character to be available */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Meta = (DisplayName = "Requires Online Item"))
	bool bRequiresItem;

	/**
	 * if set this achievement is required for this character to be available
	 * (note: achievements are currently client side only and not validated by server)
	 */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Meta = (DisplayName = "Required Offline Achievement"))
	FName RequiredAchievement;

	/** material to put on the weapon for first person view */
	UPROPERTY(EditAnywhere)
	UMaterialInstance* FPSMaterial;

	/** material to put on the weapon when not in first person */
	UPROPERTY(EditAnywhere)
	UMaterialInstance* Material;

	UPROPERTY(EditAnywhere, Meta = (MetaClass = "UTWeapon"))
	FStringClassReference WeaponType;

	UPROPERTY(EditAnywhere, Meta = (MetaClass = "UTWeapon"))
	FName WeaponSkinCustomizationTag;

};