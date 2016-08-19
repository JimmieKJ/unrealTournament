// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCarriedObject.h"
#include "UTGhostFlag.generated.h"

USTRUCT()
struct FGhostMaster
{
	GENERATED_USTRUCT_BODY()

		UPROPERTY(BlueprintReadOnly)
		AUTCarriedObject* MyCarriedObject;

	UPROPERTY()
		FVector_NetQuantize FlagLocation;

	UPROPERTY()
		FVector_NetQuantize MidPoints[NUM_MIDPOINTS];
};

UCLASS(meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTGhostFlag : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	UParticleSystemComponent* TimerEffect;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnSetCarriedObject)
		FGhostMaster GhostMaster;

	UPROPERTY()
	class AUTFlagReturnTrail* Trail;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AUTFlagReturnTrail> TrailClass;

	UPROPERTY()
	int32 TeamIndex;

	UPROPERTY()
	float TrailSpawnTime;

	UFUNCTION()
	virtual void OnSetCarriedObject();

	virtual void SetCarriedObject(AUTCarriedObject* NewCarriedObject, const FFlagTrailPos NewPosition);
	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;
};