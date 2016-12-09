// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
 *
 * Helper class to force object references for various reasons.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ObjectReferencer.generated.h"

UCLASS()
class UObjectReferencer : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Array of objects being referenced. */
	UPROPERTY(EditAnywhere, Category=ObjectReferencer)
	TArray<class UObject*> ReferencedObjects;

};

