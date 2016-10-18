// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProj_Rocket.h"
#include "UTProj_RocketGrenade.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTProj_RocketGrenade : public AUTProj_Rocket
{
	GENERATED_UCLASS_BODY()

	/**Authority creates a random seed here*/
	virtual void BeginPlay() override;

	/**Spin the visual components*/
	virtual void Tick(float DeltaTime) override;

	/**The lenth of time before explosion. Starts on the first bounce*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketGrenade)
	float FuseTime;

	/**The amount of randomness applied to FuseTime*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketGrenade)
	float RandomFuseMod;

	virtual void FuseTimer();

	/**The rate the projectile is currently spinning*/
	FRotator RotationRate;

	/**The rotation rate before bouncing*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketGrenade)
	float MaxRotationRate;

	/**The rotation rate after bouncing*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketGrenade)
	float MaxBouncedRotationRate;

	/**The random velocity adjustment after bounce*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketGrenade)
	float MaxRandomBounce;

	/**Seed so the client and server will match random bounces*/
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_Seed)
	int32 Seed;
	UFUNCTION()
	void OnRep_Seed();

	FRandomStream RNGStream;

	/**Helper For getting a rotator from the replicated seed*/
	FRotator GetRandomRotator(float MinMax);
	
	/**Spins visual rocket, random bounce, and starts the fuse*/
	virtual void OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity) override;
	virtual void OnRep_Instigator() override;
};
