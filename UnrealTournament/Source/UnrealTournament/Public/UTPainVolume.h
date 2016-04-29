// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTPainVolume.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTPainVolume : public APainCausingVolume
{
	GENERATED_UCLASS_BODY()

	virtual void ActorEnteredVolume(class AActor* Other) override;
	virtual void ActorLeavingVolume(class AActor* Other) override;
	virtual void PainTimer() override;
	virtual void BeginPlay() override;

	FTimerHandle PainTimerHandle;

	/** call AddOverlayMaterial() to add any character overlay materials; this registration is required to replicate overlays */
	UFUNCTION(BlueprintNativeEvent)
		void AddOverlayMaterials(AUTGameState* GS) const;

	/** Sound played when actor enters this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
		USoundBase* EntrySound;

	/** Sound played when actor exits this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
		USoundBase* ExitSound;

	/** Pawn Velocity reduction on entry (scales velocity Z component)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CharacterMovement)
		float PawnEntryVelZScaling;

	/** Pawn braking ability in this fluid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CharacterMovement)
		float BrakingDecelerationSwimming;

	/** Effect added to the player when they are in this volume */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
		FOverlayEffect InVolumeEffect;
};