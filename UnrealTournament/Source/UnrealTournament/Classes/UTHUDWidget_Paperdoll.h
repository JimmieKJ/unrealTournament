// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_Paperdoll.generated.h"

UCLASS()
class UUTHUDWidget_Paperdoll : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	UTexture* PaperDollTexture;

	virtual void Draw(float DeltaTime);
protected:

private:
	int32 LastHealth;
	float HealthFlashOpacity;

	int32 LastArmor;
	float ArmorFlashOpacity;
};
