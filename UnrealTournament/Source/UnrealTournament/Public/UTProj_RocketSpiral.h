// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTProj_RocketSpiral.generated.h"

/**
 * 
 */
UCLASS()
class AUTProj_RocketSpiral : public AUTProjectile
{
	GENERATED_UCLASS_BODY()


	/** flocking/spiral parameters */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Rocket)
	float FlockRadius;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Rocket)
	float FlockStiffness;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Rocket)
	float FlockMaxForce;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Rocket)
	float FlockCurlForce;

	/** other rockets in the spiral group 
	 * TODO: historically, this was bad for replication because it took two server ticks to get the whole list across due to serialization ordering and such - need to see if that's still true
	 */
	UPROPERTY(BlueprintReadWrite, Replicated, Category = Rocket)
	AUTProj_RocketSpiral* Flock[2];
	/** whether to curl around the other rockets */
	UPROPERTY(BlueprintReadWrite, Replicated, Category = Rocket)
	bool bCurl;

	/** initial movement direction, used as base for spiral */
	UPROPERTY(BlueprintReadWrite, Category = Rocket)
	FVector InitialDir;

	virtual void BeginPlay() override;

	virtual void UpdateSpiral();
};
