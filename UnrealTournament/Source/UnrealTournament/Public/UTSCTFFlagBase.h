// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTSCTFFlag.h"
#include "UTCTFFlagBase.h"
#include "UTSCTFFlagBase.generated.h"

UCLASS(HideCategories = GameObject)
class UNREALTOURNAMENT_API AUTSCTFFlagBase : public AUTCTFFlagBase
{
	GENERATED_UCLASS_BODY()

	virtual void PostInitializeComponents() override;

	// If true, this flag base will be considered a scoring point.  When a UTGauntletFlag is picked up, the assoicated scoring base will
	// be triggered for capture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	bool bScoreBase;

	// The mesh that makes up this base.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Objective)
	USkeletalMeshComponent* GhostMesh;

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Called to create the necessary capture objects.
	virtual void CreateFlag();

	// Called after each round
	virtual void Reset();

	// Activate this gauntlet flag base.  If this is a score base, then it will create the fake "scoring flag".  If it's a despenser then it will create the carried object
	virtual void Activate();

	// Deactivates this gauntlet flag base.
	virtual void Deactivate();

	// This will be called after each round
	UFUNCTION()
	virtual void RoundReset();

	UPROPERTY(Replicated, replicatedUsing = OnRep_bGhostMeshVisibile)
	bool bGhostMeshVisibile;

	UFUNCTION()
	virtual void OnRep_bGhostMeshVisibile();

	virtual void PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir);

protected:
	// We override CreateCarriedObject as it's no longer used.  CreateGauntletObject() will be called by the game mode
	// at the appropriate time instead.
	virtual void CreateCarriedObject();


};