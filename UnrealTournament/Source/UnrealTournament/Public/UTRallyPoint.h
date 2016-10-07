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

	UPROPERTY(ReplicatedUsing = OnAvailableEffectChanged, BlueprintReadOnly)
		bool bShowAvailableEffect;

	/** effect when this base is being used as a defended objective */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		UParticleSystem* AvailableEffect;

	UPROPERTY()
		UParticleSystemComponent* AvailableEffectPSC;

	UFUNCTION()
		void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual void BeginPlay() override;

	virtual void FlagCarrierInVolume(bool bFCInVolume);

	UFUNCTION()
	void OnAvailableEffectChanged();

};
