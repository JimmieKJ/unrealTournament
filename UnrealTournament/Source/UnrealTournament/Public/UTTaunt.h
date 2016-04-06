// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTTaunt.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTTaunt : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly)
	FString DisplayName;

	UPROPERTY(EditDefaultsOnly)
	FString TauntAuthor;

	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* TauntMontage;

	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* FirstPersonTauntMontage;

	UPROPERTY(EditDefaultsOnly)
	bool bAllowMovementWithFirstPersonTaunt;

	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Meta = (DisplayName = "Requires Online Item"))
	bool bRequiresItem;
};