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

	UPROPERTY(EditDefaultsOnly)
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

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Cosmetic Wearer Killed Enemy"))
	virtual void OnFlashCountIncremented();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Cosmetic Wearer Got Killing Spree"))
	virtual void OnSpreeLevelChanged(int32 NewSpreeLevel);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Cosmetic Wearer Started Taunting"))
	virtual void OnWearerEmoteStarted();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Cosmetic Wearer Stopped Taunting"))
	virtual void OnWearerEmoteEnded();

	UFUNCTION(BlueprintNativeEvent)
	void OnWearerHeadshot();

	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "On Variant Selected"))
	void OnVariantSelected(int32 Variant);

	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "On Cosmetic Wearer Death"))
	void OnWearerDeath(TSubclassOf<UDamageType> DamageType);

	virtual void PreInitializeComponents() override;
	virtual void PostInitializeComponents() override;
};