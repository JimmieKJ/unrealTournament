// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_Powerups.generated.h"

UCLASS()
class UUTHUDWidget_Powerups : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** max amount of space each powerup is given (normalized)
	 * automatically reduced if more powerups need to be displayed
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = HUD)
	FVector2D MaxPowerupSize;

	virtual void Draw_Implementation(float DeltaTime);
};
