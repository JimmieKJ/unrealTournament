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
	bool bSuppressTrails;

	UPROPERTY()
	bool bShowTimer;

	UPROPERTY()
	uint8 TeamNum;

	UPROPERTY()
	FVector_NetQuantize FlagLocation;

	UPROPERTY()
	FVector_NetQuantize MidPoints[NUM_MIDPOINTS];

	FGhostMaster()
	{
		bShowTimer = true;
		bSuppressTrails = false;
		TeamNum = 255;
	}
};

UCLASS(meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTGhostFlag : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Flag)
	USkeletalMeshComponent* Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Flag)
	TArray<UMaterialInstanceDynamic*> MeshMIDs;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Flag)
	UParticleSystemComponent* TimerEffect;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnSetCarriedObject)
	FGhostMaster GhostMaster;

	UPROPERTY()
	class AUTFlagReturnTrail* Trail;

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AUTFlagReturnTrail> TrailClass;

	UPROPERTY()
	float TrailSpawnTime;

	UFUNCTION()
	virtual void OnSetCarriedObject();

	virtual void SetCarriedObject(AUTCarriedObject* NewCarriedObject, const FFlagTrailPos NewPosition);
	virtual void Tick(float DeltaTime) override;
	virtual void Destroyed() override;
	virtual void PostInitializeComponents() override;

	virtual void SetGhostColor(FLinearColor NewColor);

};