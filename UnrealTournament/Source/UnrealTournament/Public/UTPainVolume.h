// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTPainVolume.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTPainVolume : public APainCausingVolume, public IInterface_PostProcessVolume
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

	/** Post process settings to use for the included PostProcessEffect volume. */
	UPROPERTY(interp, Category = PostProcessVolume, meta = (ShowOnlyInnerProperties))
	struct FPostProcessSettings Settings;
	
	/** World space radius around the volume that is used for blending (only if not unbound).			*/
	UPROPERTY(interp, Category = PostProcessVolume, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "6000.0"))
		float BlendRadius;

	/** 0:no effect, 1:full effect */
	UPROPERTY(interp, Category = PostProcessVolume, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "1.0"))
		float BlendWeight;

	/** Immune team index */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CharacterMovement)
		int32 ImmuneTeamIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
		FText VolumeName;

	//~ Begin IInterface_PostProcessVolume Interface
	virtual bool EncompassesPoint(FVector Point, float SphereRadius/*=0.f*/, float* OutDistanceToPoint) override;
	virtual FPostProcessVolumeProperties GetProperties() const override
	{
		FPostProcessVolumeProperties Ret;
		Ret.bIsEnabled = true;
		Ret.bIsUnbound = false;
		Ret.BlendRadius = BlendRadius;
		Ret.BlendWeight = BlendWeight;
		Ret.Priority = Priority;
		Ret.Settings = &Settings;
		return Ret;
	}
	//~ End IInterface_PostProcessVolume Interface

	//~ Begin AActor Interface
	virtual void PostUnregisterAllComponents(void) override;
	virtual void CausePainTo(class AActor* Other) override;

protected:
	virtual void PostRegisterAllComponents() override;
	//~ End AActor Interface
};