// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTGroupTaunt.generated.h"

UCLASS(Blueprintable, Abstract, CustomConstructor)
class UNREALTOURNAMENT_API AUTGroupTaunt : public AActor
{
	GENERATED_BODY()

public:
	AUTGroupTaunt()
	{

	}

	UPROPERTY(EditDefaultsOnly)
	FString DisplayName;

	UPROPERTY(EditDefaultsOnly)
	FString TauntAuthor;

	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* TauntMontage;

	UPROPERTY(EditDefaultsOnly)
	USoundBase* BGMusic;

	UPROPERTY(EditDefaultsOnly)
	bool bCascading;

	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, Meta = (DisplayName = "Requires Online Item"))
	bool bRequiresItem;
};