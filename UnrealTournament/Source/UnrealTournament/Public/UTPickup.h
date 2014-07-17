// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPickup.generated.h"

UCLASS(abstract, Blueprintable, meta = (ChildCanTick))
class AUTPickup : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	TSubobjectPtr<UCapsuleComponent> Collision;
	// hack: UMaterialBillboardComponent isn't exposed, can't use native subobject
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = Pickup)
	UMaterialBillboardComponent* TimerSprite;
	//UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	//TSubobjectPtr<UMaterialBillboardComponent> TimerSprite;
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	TSubobjectPtr<UTextRenderComponent> TimerText;

	/** respawn time for the pickup; if it's <= 0 then the pickup doesn't respawn until the round resets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pickup)
	float RespawnTime;
	/** if set, pickup begins play with its respawn time active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pickup)
	uint32 bDelayedSpawn : 1;
	/** whether the pickup is currently active */
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_bActive, Category = Pickup)
	uint32 bActive : 1;
	/** whether to display TimerSprite/TimerText on the pickup while it is respawning */
	uint32 bDisplayRespawnTimer : 1;
	/** plays taken effects when received on client as true */
	UPROPERTY(ReplicatedUsing=ReplicatedTakenEffects)
	uint32 bRepTakenEffects : 1;
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
	/** all components with any of these tags will be hidden when the pickup is taken
	 * if the array is empty, the entire pickup Actor is hidden
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Effects)
	TArray<FName> TakenHideTags;

	UPROPERTY(BlueprintReadOnly, Category = Effects)
	UMaterialInstanceDynamic* TimerMI;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished);
#endif
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	virtual void OnRep_bActive();
	UFUNCTION()
	void ReplicatedTakenEffects();

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult);

	UFUNCTION(BlueprintNativeEvent)
	void ProcessTouch(APawn* TouchedBy);

	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	void GiveTo(APawn* Target);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Pickup)
	void StartSleeping();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Pickup)
	void WakeUp();
	/** used for the timer-based call to WakeUp() so clients can perform different behavior to handle possible sync issues */
	UFUNCTION()
	void WakeUpTimer();
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void PlayTakenEffects(bool bReplicate);
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void PlayRespawnEffects();
	/** sets the hidden state of the pickup - note that this doesn't necessarily mean the whole object (e.g. item mesh but not holder) */
	UFUNCTION(BlueprintCallable, Category = Pickup)
	virtual void SetPickupHidden(bool bNowHidden);

	virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker);
	
protected:
	/** used to replicate remaining respawn time to newly joining clients */
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_RespawnTimeRemaining)
	float RespawnTimeRemaining;

	UFUNCTION()
	virtual void OnRep_RespawnTimeRemaining();
};
