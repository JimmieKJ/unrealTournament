// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProj_BioGrenade.generated.h"

UCLASS()
class AUTProj_BioGrenade : public AUTProjectile
{
	GENERATED_BODY()
public:
	AUTProj_BioGrenade(const FObjectInitializer& OI);

	UPROPERTY(VisibleDefaultsOnly)
	USphereComponent* ProximitySphere;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = StartFuse)
	bool bBeginFuseWarning;

	/** effect attached to projectile when fuse is lit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UParticleSystem* FuseEffect;
	/** sound played periodically while fuse is lit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	USoundBase* FuseBeepSound;

	/** maximum amount of time before starting explosion sequence (may start earlier if it gets close to a target or runs out of bounces) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TimeBeforeFuseStarts;
	/** amount of time fuse is active before exploding */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float FuseTime;
	/** delay on spawn before we consider nearby targets for early fuse start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ProximityDelay;

	UFUNCTION(BlueprintCallable, Category = BioGrenade)
	virtual void StartFuse();
	UFUNCTION()
	virtual void StartFuseTimed();
	UFUNCTION()
	virtual void FuseExpired();
	UFUNCTION()
	virtual void PlayFuseBeep();

	virtual void BeginPlay() override;
	virtual void OnStop(const FHitResult& Hit) override;
	UFUNCTION()
	virtual void ProximityOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};