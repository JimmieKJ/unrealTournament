// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeapon.h"

#include "UTWeap_ShockRifle.generated.h"

UCLASS(abstract)
class UNREALTOURNAMENT_API AUTWeap_ShockRifle : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	/** shock ball bot is waiting to combo */
	UPROPERTY()
	class AUTProj_ShockBall* ComboTarget;
	/** set when bot intends to throw a prediction shot at an enemy not currently visible */
	UPROPERTY()
	FVector PredictiveComboTargetLoc;
	/** last time we did the expensive prediction check */
	UPROPERTY()
	float LastPredictiveComboCheckTime;
	/** set when bot wants next shock ball fired to set up combo */
	UPROPERTY()
	bool bPlanningCombo;
	/** result of MovingComboCheck() when bot starts a potential combo (one result for whole combo since the function returns a random skill check) */
	UPROPERTY()
	bool bMovingComboCheckResult;
	
	/** effects flag to play combo-specific animations and effects (clientside guess, not necessarily 100% accurate) */
	bool bPlayComboEffects;

	/** render target for on-mesh display screen */
	UPROPERTY(BlueprintReadWrite, Category = Mesh)
	UCanvasRenderTarget2D* ScreenTexture;
	/** drawn to the display screen for a short time after holder kills an enemy */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Mesh)
	UTexture2D* ScreenKillNotifyTexture;
	/** font for text on the display screen */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Mesh)
	UFont* ScreenFont;
	/** material ID for the screen */
	UPROPERTY(EditDefaultsOnly, Category = Mesh)
	int32 ScreenMaterialID;
	/** material instance showing the screen */
	UPROPERTY(BlueprintReadWrite, Category = Mesh)
	UMaterialInstanceDynamic* ScreenMI;

	/** anim to play for a shock combo instead of the normal one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* ComboFireAnim;

	TSharedPtr<FCanvasWordWrapper> WordWrapper;

	/** set (client only) to last time user recorded a kill while holding this weapon, used to change on-mesh screen display */
	UPROPERTY(BlueprintReadWrite, Category = Mesh)
	float LastClientKillTime;
	
	virtual void AttachToOwner_Implementation() override;
	UFUNCTION()
	virtual void UpdateScreenTexture(UCanvas* C, int32 Width, int32 Height);
	virtual void Tick(float DeltaTime) override;

	virtual void NotifyKillWhileHolding_Implementation(TSubclassOf<UDamageType> DmgType) override
	{
		LastClientKillTime = GetWorld()->TimeSeconds;
	}

	virtual UAnimMontage* GetFiringAnim(uint8 FireMode) const;
	virtual void PlayFiringEffects();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic)
	void Play1PComboEffects();

	virtual void HitScanTrace(const FVector& StartLocation, const FVector& EndTrace, FHitResult& Hit, float PredictionTime) override;
	virtual AUTProjectile* FireProjectile() override;

	/** returns whether AI using this weapon shouldn't fire because it's waiting for a combo trigger */
	virtual bool WaitingForCombo();

	virtual void DoCombo();

	virtual float SuggestAttackStyle_Implementation() override;
	virtual float GetAISelectRating_Implementation() override;
	virtual bool CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc) override;

	virtual bool IsPreparingAttack_Implementation() override;
	virtual bool ShouldAIDelayFiring_Implementation() override;

	virtual int32 GetWeaponKillStats(AUTPlayerState * PS) const override;
	virtual int32 GetWeaponDeathStats(AUTPlayerState * PS) const override;

	virtual void FireInstantHit(bool bDealDamage, FHitResult* OutHit) override;
	virtual void FiringExtraUpdated_Implementation(uint8 NewFlashExtra, uint8 InFireMode) override;
};