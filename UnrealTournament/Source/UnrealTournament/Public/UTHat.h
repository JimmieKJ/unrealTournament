// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHat.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTHat : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly)
	FString HatName;

	UPROPERTY(EditDefaultsOnly)
	FString HatAuthor;

	UPROPERTY(BlueprintReadOnly, Category=UTHat)
	AUTCharacter* HatWearer;

	UFUNCTION(BlueprintImplementableEvent, meta = (FriendlyName = "On Hat Wearer Killed Enemy"))
	virtual void OnFlashCountIncremented();

	UFUNCTION(BlueprintImplementableEvent, meta = (FriendlyName = "On Hat Wearer Got Killing Spree"))
	virtual void OnSpreeLevelChanged(int32 NewSpreeLevel);

	UFUNCTION(BlueprintImplementableEvent, meta = (FriendlyName = "On Hat Wearer Started Taunting"))
	virtual void OnWearerEmoteStarted();

	UFUNCTION(BlueprintImplementableEvent, meta = (FriendlyName = "On Hat Wearer Stopped Taunting"))
	virtual void OnWearerEmoteEnded();

	virtual void PreInitializeComponents();
};
