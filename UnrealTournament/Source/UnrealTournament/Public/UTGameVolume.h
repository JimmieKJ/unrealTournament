// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTGameVolume.generated.h"

/**
* Type of physics volume that has UT specific gameplay effects/data.
*/

UCLASS(BlueprintType)
class UNREALTOURNAMENT_API AUTGameVolume : public APhysicsVolume
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		FText VolumeName;
};


