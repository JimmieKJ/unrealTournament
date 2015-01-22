// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/* epic ===============================================
* class BookMark
*
* A camera position the current level.
 */

#pragma once
#include "BookMark.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UBookMark : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Camera position */
	UPROPERTY(EditAnywhere, Category=BookMark)
	FVector Location;

	/** Camera rotation */
	UPROPERTY(EditAnywhere, Category=BookMark)
	FRotator Rotation;

	/** Array of levels that are hidden */
	UPROPERTY(EditAnywhere, Category=BookMark)
	TArray<FString> HiddenLevels;

};

