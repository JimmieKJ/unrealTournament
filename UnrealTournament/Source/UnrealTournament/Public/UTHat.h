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

	UFUNCTION(BlueprintImplementableEvent)
	virtual void OnFlashCountIncremented();

	UFUNCTION(BlueprintImplementableEvent)
	virtual void OnSpreeLevelChanged(int32 NewSpreeLevel);

	virtual void PreInitializeComponents();
};
