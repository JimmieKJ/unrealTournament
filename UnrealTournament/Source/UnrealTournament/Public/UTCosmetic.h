// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCosmetic.generated.h"

USTRUCT(BlueprintType)
struct FColorSwap
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly)
	FLinearColor Color1;

	UPROPERTY(EditDefaultsOnly)
	FLinearColor Color2;
};

UCLASS(Abstract)
class UNREALTOURNAMENT_API AUTCosmetic : public AActor
{
	GENERATED_UCLASS_BODY()

	/** if set a UTProfileItem is required for this character to be available */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Meta = (DisplayName = "Requires Online Item"))
	bool bRequiresItem;
	/** if set this achievement is required for this character to be available
	 * (note: achievements are currently client side only and not validated by server)
	 */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Meta = (DisplayName = "Required Offline Achievement"))
	FName RequiredAchievement;

	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable)
	FString CosmeticName;

	UPROPERTY(EditDefaultsOnly)
	FString CosmeticAuthor;

	UPROPERTY(BlueprintReadOnly, Category = UTHat)
	AUTCharacter* CosmeticWearer;

	UPROPERTY(BlueprintReadOnly)
	TArray<UMaterialInstanceDynamic*> CosmeticMIs;

	UPROPERTY(EditDefaultsOnly)
	TArray<FText> VariantNames;
	
	UPROPERTY(EditDefaultsOnly)
	TArray<FColorSwap> VariantColorSwaps;

	UPROPERTY(EditDefaultsOnly)
	bool bHideWithOverlay;

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Cosmetic Wearer Killed Enemy"))
	void OnFlashCountIncremented();
	
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Cosmetic Wearer Got Killing Spree"))
	void OnSpreeLevelChanged(int32 NewSpreeLevel);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Cosmetic Wearer Started Taunting"))
	void OnWearerEmoteStarted();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Cosmetic Wearer Stopped Taunting"))
	void OnWearerEmoteEnded();

	UFUNCTION(BlueprintNativeEvent)
	void OnWearerHeadshot();

	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "On Variant Selected"))
	void OnVariantSelected(int32 Variant);

	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "On Cosmetic Wearer Death"))
	void OnWearerDeath(TSubclassOf<UDamageType> DamageType);

	UFUNCTION(BlueprintCallable, Category="Cosmetic")
	virtual void SetBodiesToSimulatePhysics();

	UFUNCTION(BlueprintNativeEvent)
	TSubclassOf<AUTGib> OverrideGib(FName BoneName);

	virtual void PreInitializeComponents() override;
	virtual void PostInitializeComponents() override;
};