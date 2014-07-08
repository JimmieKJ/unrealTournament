// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProj_BioShot.h"
#include "UTProj_BioGlobling.generated.h"

/**
 * 
 */
UCLASS()
class AUTProj_BioGlobling : public AUTProj_BioShot
{
	GENERATED_UCLASS_BODY()

	/**Overridden to pass through Globs*/
	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);
};
