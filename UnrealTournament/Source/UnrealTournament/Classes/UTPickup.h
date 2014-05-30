// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPickup.generated.h"

UCLASS(abstract, Blueprintable, meta = (ChildCanTick))
class AUTPickup : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	TSubobjectPtr<UCapsuleComponent> Collision;

	/** respawn time for the pickup; if it's <= 0 then the pickup doesn't respawn until the round resets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pickup)
	float RespawnTime;
	/** if set, pickup begins play with its respawn time active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pickup)
	bool bDelayedSpawn;
	/** whether the pickup is currently active */
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing=OnRep_bActive, Category = Pickup)
	bool bActive;
	/** one-shot particle effect played when the pickup is taken */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	UParticleSystem* TakenParticles;
	/** one-shot sound played when the pickup is taken */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	USoundBase* TakenSound;
	/** one-shot particle effect played when the pickup respawns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	UParticleSystem* RespawnParticles;
	/** one-shot sound played when the pickup respawns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	USoundBase* RespawnSound;

	UFUNCTION()
	virtual void OnRep_bActive();

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION(BlueprintNativeEvent)
	void ProcessTouch(APawn* TouchedBy);

	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void GiveTo(APawn* Target);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Pickup)
	void StartSleeping();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Pickup)
	void WakeUp();
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void PlayTakenEffects();
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void PlayRespawnEffects();
	/** sets the hidden state of the pickup - note that this doesn't necessarily mean the whole object (e.g. item mesh but not holder) */
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void SetPickupHidden(bool bNowHidden);
};
