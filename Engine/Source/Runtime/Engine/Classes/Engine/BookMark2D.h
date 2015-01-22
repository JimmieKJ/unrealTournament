// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 *
 * Simple class to store 2D camera information.
 */

#pragma once
#include "BookMark2D.generated.h"

UCLASS(hidecategories=Object)
class UBookMark2D : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Zoom of the camera */
	UPROPERTY(EditAnywhere, Category=BookMark2D)
	float Zoom2D;

	/** Location of the camera */
	UPROPERTY(EditAnywhere, Category=BookMark2D)
	FIntPoint Location;

};

