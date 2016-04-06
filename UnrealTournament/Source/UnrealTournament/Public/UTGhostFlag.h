// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCarriedObject.h"
#include "UTGhostFlag.generated.h"

UCLASS(meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTGhostFlag : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	UParticleSystemComponent* TimerEffect;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Flag)
	AUTCarriedObject* MyCarriedObject;

	virtual void Tick(float DeltaTime) override;

};