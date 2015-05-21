// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_WeaponCrosshair.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_WeaponCrosshair : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:

	virtual void Draw_Implementation(float DeltaTime);
protected:

private:
};
