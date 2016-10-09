// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTRallyPoint.generated.h"

UCLASS(Blueprintable)
class UNREALTOURNAMENT_API AUTRallyPoint : public AUTGameObjective
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Objective)
		UCapsuleComponent* Capsule;

	UPROPERTY(BlueprintReadOnly)
		class AUTGameVolume* MyGameVolume;

	UPROPERTY(BlueprintReadOnly)
		class AUTCharacter* NearbyFC;

	/** how long FC has to be here for rally to start */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		float RallyReadyDelay;

	UPROPERTY(ReplicatedUsing = OnAvailableEffectChanged, BlueprintReadOnly)
		bool bShowAvailableEffect;

	/** effect when this base is being used as a defended objective */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		UParticleSystem* AvailableEffect;

	UPROPERTY()
		UParticleSystemComponent* AvailableEffectPSC;

	/** effect when this base is being used as a defended objective */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		UParticleSystem* RallyEffect;

	UPROPERTY()
		UParticleSystemComponent* RallyEffectPSC;

	UFUNCTION()
		void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	virtual void FlagCarrierInVolume(class AUTCharacter* NewFC);

	UFUNCTION()
	void OnAvailableEffectChanged();

};
