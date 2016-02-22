// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTNoCameraVolume.generated.h"

/**
* PhysicsVolume: A bounding volume which affects actor physics
* Each AActor is affected at any time by one PhysicsVolume
*/

UCLASS(BlueprintType)
class UNREALTOURNAMENT_API AUTNoCameraVolume : public APhysicsVolume
{
	GENERATED_UCLASS_BODY()
};
