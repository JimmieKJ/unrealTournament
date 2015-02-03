// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCosmetic.generated.h"

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

	UFUNCTION(BlueprintImplementableEvent, meta = (FriendlyName = "On Cosmetic Wearer Killed Enemy"))
	virtual void OnFlashCountIncremented();

	UFUNCTION(BlueprintImplementableEvent, meta = (FriendlyName = "On Cosmetic Wearer Got Killing Spree"))
	virtual void OnSpreeLevelChanged(int32 NewSpreeLevel);

	UFUNCTION(BlueprintImplementableEvent, meta = (FriendlyName = "On Cosmetic Wearer Started Taunting"))
	virtual void OnWearerEmoteStarted();

	UFUNCTION(BlueprintImplementableEvent, meta = (FriendlyName = "On Cosmetic Wearer Stopped Taunting"))
	virtual void OnWearerEmoteEnded();

	UFUNCTION(BlueprintNativeEvent)
	void OnWearerHeadshot();

	virtual void PreInitializeComponents() override;
};