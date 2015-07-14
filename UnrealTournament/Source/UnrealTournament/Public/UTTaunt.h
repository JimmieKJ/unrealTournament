// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	bool bRequiresItem;
};