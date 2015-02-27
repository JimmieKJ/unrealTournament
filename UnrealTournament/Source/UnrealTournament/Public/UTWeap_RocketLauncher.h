// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeapon.h"
#include "UTProj_RocketSeeking.h"
#include "UTWeap_RocketLauncher.generated.h"

USTRUCT()
struct FRocketFireMode
{
	GENERATED_USTRUCT_BODY()

	/** The string shown in the HUD when this firemode is selected*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rocket)
	FText DisplayString;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rocket)
	TSubclassOf<AUTProjectile> ProjClass;

	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rocket)
	USoundBase* FireSound;

	/** Can this Rocket mode be a seeking rocket? No for grenade*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rocket)
	bool bCanBeSeekingRocket;

	/** Should show muzzle flashes when fired? No for grenade */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rocket)
	bool bCauseMuzzleFlash;

	/** Spread for this firemode. Does Different things for each mode*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rocket)
	float Spread;
};
/**
 * Rocket Launcher
 *
 * Needs 3 muzzle flash locations for each barrel
 */
UCLASS(abstract)
class UNREALTOURNAMENT_API AUTWeap_RocketLauncher : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	/**The loading animations per NumLoadedRockets*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	TArray<UAnimMontage*> LoadingAnimation;

	/**The sounds to play when loading each rocket*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	TArray<USoundBase*> LoadingSounds;

	/**The sound that indicates a rocket was loaded*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	USoundBase* RocketLoadedSound;

	/** The sound to play when alt-fire mode is changed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	USoundBase* AltFireModeChangeSound;

	/**The firing animations per NumLoadedRockets*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	TArray<UAnimMontage*> FiringAnimation;

	/**The textures used for drawing the HUD*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	TArray<UTexture2D*> LoadCrosshairTextures;

	/**The texture for locking on a target*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	UTexture2D* LockCrosshairTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	float CrosshairScale;

	/**The time it takes to rotate the crosshair when loading a rocket*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	float CrosshairRotationTime;

	/**The last time we loaded a rocket. For the HUD crosshair*/
	float LastLoadTime;

	float CurrentRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	TArray<FRocketFireMode> RocketFireModes;
	int32 CurrentRocketFireMode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	TSubclassOf<AUTProj_RocketSeeking> SeekingProjClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	int32 MaxLoadedRockets;
	UPROPERTY(BlueprintReadOnly, Category = RocketLauncher)
	int32 NumLoadedRockets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	float RocketLoadTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	float FirstRocketLoadTime;

	/** How much grace at the end of loading should be given before the weapon auto-fires */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	float GracePeriod;

	/** Burst rocket firing interval */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
		float BurstInterval;

	/**Distance from the center of the launcher to where the rockets fire from*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	float BarrelRadius;

	UFUNCTION(BlueprintCallable, Category = Firing)
	virtual AUTProjectile* FireRocketProjectile();

	virtual TSubclassOf<AUTProjectile> GetRocketProjectile();
	virtual float GetLoadTime();
	virtual void BeginLoadRocket();
	virtual void EndLoadRocket();
	virtual void ClearLoadedRockets();
	/** called by server to tell client to stop loading rockets early, generally because we're out of ammo */
	UFUNCTION(Reliable, Client)
	virtual void ClientAbortLoad();
	virtual float GetSpread(int32 ModeIndex);
	virtual void FireShot() override;
	virtual AUTProjectile* FireProjectile() override;
	virtual void PlayFiringEffects() override;
	virtual void OnMultiPress_Implementation(uint8 OtherFireMode) override;
	
	/**HUD*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	UFont* RocketModeFont;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RocketLauncher)
	float UnderReticlePadding;
	bool  bDrawRocketModeString;

	virtual void DrawWeaponCrosshair_Implementation(UUTHUDWidget* WeaponHudWidget, float RenderDelta) override;


	//Targeting
	virtual void StateChanged() override;

	FTimerHandle UpdateLockHandle;
	virtual void UpdateLock();
	virtual void SetLockTarget(AActor* NewTarget);
	virtual bool CanLockTarget(AActor* Target);
	bool HasLockedTarget() 
	{ 
		return bLockedOnTarget && LockedTarget != NULL;
	}

	bool WithinLockAim(AActor *Target);

	/** The frequency with which we will check for a lock */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lock)
	float		LockCheckTime;

	/** How far out should we be considering actors for a lock */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lock)
	float		LockRange;

	/** How long does the player need to target an actor to lock on to it*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lock)
	float		LockAcquireTime;

	/** Once locked, how long can the player go without painting the object before they lose the lock */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lock)
	float		LockTolerance;

	/** When true, this weapon is locked on target */
	UPROPERTY(BlueprintReadWrite, Category = Lock)
	bool 		bLockedOnTarget;

	/** What "target" is this weapon locked on to */
	UPROPERTY(BlueprintReadWrite, Replicated, ReplicatedUsing = OnRep_LockedTarget, Category = Lock)
	AActor* 				LockedTarget;
	UFUNCTION()
	virtual void OnRep_LockedTarget();

	/** What "target" is current pending to be locked on to */
	UPROPERTY(BlueprintReadWrite, Category = Lock)
	AActor*				PendingLockedTarget;

	/** How long since the Lock Target has been valid */
	UPROPERTY(BlueprintReadWrite, Category = Lock)
	float  				LastLockedOnTime;

	/** When did the pending Target become valid */
	UPROPERTY(BlueprintReadWrite, Category = Lock)
	float				PendingLockedTargetTime;

	/** When was the last time we had a valid target */
	UPROPERTY(BlueprintReadWrite, Category = Lock)
	float				LastValidTargetTime;

	/** angle for locking for lock targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lock)
	float 				LockAim;


	/** Sound Effects to play when Locking */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lock)
	USoundBase*  			LockAcquiredSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lock)
	USoundBase* 			LockLostSound;

	/** If true, weapon will try to lock onto targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Lock)
	bool bTargetLockingActive;

	/** Last time target lock was checked */
	UPROPERTY(BlueprintReadWrite, Category = Lock)
	float LastTargetLockCheckTime;

	virtual float SuggestAttackStyle_Implementation() override;
	virtual float GetAISelectRating_Implementation() override;
	virtual bool CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc) override;

	virtual bool IsPreparingAttack_Implementation() override;

	virtual void Destroyed() override;
};
