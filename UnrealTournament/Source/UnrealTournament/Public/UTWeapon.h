// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTInventory.h"
#include "UTProjectile.h"
#include "UTATypes.h"
#include "UTPlayerController.h"

#include "UTWeapon.generated.h"

USTRUCT(BlueprintType)
struct FInstantHitDamageInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	int32 Damage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	TSubclassOf<UDamageType> DamageType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	float Momentum;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DamageInfo")
	float TraceRange;

	FInstantHitDamageInfo()
		: Damage(10), TraceRange(25000.0f)
	{}
};

USTRUCT()
struct FDelayedProjectileInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TSubclassOf<AUTProjectile> ProjectileClass;

	UPROPERTY()
	FVector SpawnLocation;

	UPROPERTY()
	FRotator SpawnRotation;

	FDelayedProjectileInfo()
		: ProjectileClass(NULL), SpawnLocation(ForceInit), SpawnRotation(ForceInit)
	{}
};

USTRUCT()
struct FDelayedHitScanInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector ImpactLocation;

	UPROPERTY()
	uint8 FireMode;

	UPROPERTY()
	FVector SpawnLocation;

	UPROPERTY()
	FRotator SpawnRotation;

	FDelayedHitScanInfo()
		: ImpactLocation(ForceInit), FireMode(0), SpawnLocation(ForceInit), SpawnRotation(ForceInit)
	{}
};

UCLASS(Blueprintable, Abstract, NotPlaceable, Config = Game)
class UNREALTOURNAMENT_API AUTWeapon : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	friend class UUTWeaponState;
	friend class UUTWeaponStateInactive;
	friend class UUTWeaponStateActive;
	friend class UUTWeaponStateEquipping;
	friend class UUTWeaponStateUnequipping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, ReplicatedUsing = OnRep_AttachmentType, Category = "Weapon")
	TSubclassOf<class AUTWeaponAttachment> AttachmentType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, ReplicatedUsing = OnRep_Ammo, Category = "Weapon")
	int32 Ammo;

	UFUNCTION()
	virtual void OnRep_AttachmentType();

	/** handles weapon switch when out of ammo, etc
	 * NOTE: called on server if owner is locally controlled, on client only when owner is remote
	 */
	UFUNCTION()
	virtual void OnRep_Ammo();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Weapon")
	int32 MaxAmmo;

	/** ammo cost for one shot of each fire mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<int32> AmmoCost;

	/** projectile class for fire mode (if applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray< TSubclassOf<AUTProjectile> > ProjClass;

	/** instant hit data for fire mode (if applicable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<FInstantHitDamageInfo> InstantHitInfo;

	/** firing state for mode, contains core firing sequence and directs to appropriate global firing functions */
	UPROPERTY(Instanced, EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "Weapon", NoClear)
	TArray<class UUTWeaponStateFiring*> FiringState;

	/** True for melee weapons affected by "stopping power" (momentum added for weapons that don't normally impart much momentum) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bAffectedByStoppingPower;

	/** Custom Momentum scaling for friendly hitscanned pawns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float FriendlyMomentumScaling;

	virtual	float GetImpartedMomentumMag(AActor* HitActor);

	virtual void Serialize(FArchive& Ar) override
	{
		// prevent AutoSwitchPriority from being serialized using non-config paths
		// without this any local user setting will get pushed to blueprints and then override other users' configuration
		float SavedSwitchPriority = AutoSwitchPriority;
		Super::Serialize(Ar);
		AutoSwitchPriority = SavedSwitchPriority;
	}

	/** Synchronize random seed on server and firing client so projectiles stay synchronized */
	virtual void NetSynchRandomSeed();

	/** time between shots, trigger checks, etc */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.1"))
	TArray<float> FireInterval;
	/** firing spread (random angle added to shots) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<float> Spread;
	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<USoundBase*> FireSound;
	/** looping (ambient) sound to set on owner while firing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<USoundBase*> FireLoopingSound;
	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<UAnimMontage*> FireAnimation;
	/** particle component for muzzle flash */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TArray<UParticleSystemComponent*> MuzzleFlash;
	/** saved transforms of MuzzleFlash components used for UpdateWeaponHand() to know original values from the blueprint */
	UPROPERTY(Transient)
	TArray<FTransform> MuzzleFlashDefaultTransforms;
	/** saved sockets for MuzzleFlash components used for UpdateWeaponHand() to know original values from the blueprint */
	UPROPERTY(Transient)
	TArray<FName> MuzzleFlashSocketNames;
	/** particle system for firing effects (instant hit beam and such)
	 * particles will be sourced at FireOffset and a parameter HitLocation will be set for the target, if applicable
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TArray<UParticleSystem*> FireEffect;

	/** Fire Effect happens once every FireEffectInterval shots */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	int32 FireEffectInterval;
	/** shots since last fire effect. */
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	int32 FireEffectCount;
	/** optional effect for instant hit endpoint */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TArray< TSubclassOf<class AUTImpactEffect> > ImpactEffect;
	/** throttling for impact effects - don't spawn another unless last effect is farther than this away or longer ago than MaxImpactEffectSkipTime */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	float ImpactEffectSkipDistance;
	/** throttling for impact effects - don't spawn another unless last effect is farther than ImpactEffectSkipDistance away or longer ago than this */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	float MaxImpactEffectSkipTime;
	/** FlashLocation for last played impact effect */
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	FVector LastImpactEffectLocation;
	/** last time an impact effect was played */
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	float LastImpactEffectTime;
	/** materials on weapon mesh first time we change its skin, used to preserve any runtime blueprint changes */
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	TArray<UMaterialInterface*> SavedMeshMaterials;

	/** If true, weapon is visibly holstered when not active.  There can only be one holstered weapon in inventory at a time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	bool bMustBeHolstered;

	/** If true , weapon can be thrown. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Weapon")
	bool bCanThrowWeapon;

	/** if true, don't display in menus like the weapon priority menu (generally because the weapon's use is outside the user's control, e.g. instagib) */
	UPROPERTY(EditDefaultsOnly, Category = UI)
	bool bHideInMenus;

	/** Hack for adjusting first person weapon mesh at different FOVs (until we have separate render pass for first person weapon. */
	UPROPERTY()
	FVector FOVOffset;

	UFUNCTION()
	virtual void AttachToHolster();

	UFUNCTION()
	virtual void DetachFromHolster();

	virtual void DropFrom(const FVector& StartLocation, const FVector& TossVelocity) override;

	/** first person mesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	USkeletalMeshComponent* Mesh;

	USkeletalMeshComponent* GetMesh() const { return Mesh; }

	/** causes weapons fire to originate from the center of the player's view when in first person mode (and human controlled)
	 * in other cases the fire start point defaults to the weapon's world position
	 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	bool bFPFireFromCenter;
	/** if set ignore FireOffset for instant hit fire modes when in first person mode */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	bool bFPIgnoreInstantHitFireOffset;
	/** Firing offset from weapon for weapons fire. If bFPFireFromCenter is true and it's a player in first person mode, this is from the camera center */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FVector FireOffset;

	/** If true (on server), use the last bSpawnedShot saved position as starting point for this shot to synch with client firing position. */
	UPROPERTY()
	bool bNetDelayedShot;

	/** indicates this weapon is most useful in melee range (used by AI) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	bool bMeleeWeapon;
	/** indicates AI should prioritize accuracy over evasion (low skill bots will stop moving, higher skill bots prioritize strafing and avoid actions that move enemy across view */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	bool bPrioritizeAccuracy;
	/** indicates AI should target for splash damage (e.g. shoot at feet or nearby walls) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	bool bRecommendSplashDamage;
	/** indicates AI should consider firing at enemies that aren't currently visible but are close to being so
	 * generally set for rapid fire weapons or weapons with spinup/spindown costs
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	bool bRecommendSuppressiveFire;
	/** indicates this is a sniping weapon (for AI, will prioritize headshots and long range targeting) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	bool bSniping;

	/** Delayed projectile information */
	UPROPERTY()
	FDelayedProjectileInfo DelayedProjectile;

	/** Delayed hitscan information */
	UPROPERTY()
	FDelayedHitScanInfo DelayedHitScan;

	/** return whether to play first person visual effects
	 * not as trivial as it sounds as we need to appropriately handle bots (never actually 1P), first person spectating (can be 1P even though it's a remote client), hidden weapons setting (should draw even though weapon mesh is hidden), etc
	 */
	UFUNCTION(BlueprintCallable, Category = Effects)
	bool ShouldPlay1PVisuals() const;

	/** Play impact effects client-side for predicted hitscan shot - decides whether to delay because of high client ping. */
	virtual void PlayPredictedImpactEffects(FVector ImpactLoc);

	FTimerHandle PlayDelayedImpactEffectsHandle;

	/** Trigger delayed hitscan effects, delayed because client ping above max forward prediction limit. */
	virtual void PlayDelayedImpactEffects();

	FTimerHandle SpawnDelayedFakeProjHandle;

	/** Spawn a delayed projectile, delayed because client ping above max forward prediction limit. */
	virtual void SpawnDelayedFakeProjectile();

	/** time to bring up the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float BringUpTime;
	/** time to put down the weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float PutDownTime;

	/** Earliest time can fire again (failsafe for weapon swapping). */
	UPROPERTY()
	float EarliestFireTime;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual float GetBringUpTime();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual float GetPutDownTime();

	/** equip anims */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* BringUpAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	UAnimMontage* PutDownAnim;

	/** weapon group - NextWeapon() picks the next highest group, PrevWeapon() the next lowest, etc
	 * generally, the corresponding number key is bound to access the weapons in that group
	 */
	UPROPERTY(EditAnywhere, Config, Transient, BlueprintReadOnly, Category = "Selection")
	int32 Group;

	/** Group this weapon was assigned to in past UTs when each weapon was in its own slot. */
	UPROPERTY()
	int32 ClassicGroup;

	/** if the player acquires more than one weapon in a group, we assign a unique GroupSlot to keep a consistent order
	 * this value is only set on clients
	 */
	UPROPERTY(EditAnywhere, Config, Transient, BlueprintReadOnly, Category = "Selection")
	int32 GroupSlot;

	/** Returns true if weapon follows OtherWeapon in the weapon list (used for nextweapon/previousweapon) */
	virtual bool FollowsInList(AUTWeapon* OtherWeapon, bool bUseClassicGroups);

	/** user set priority for auto switching and switch to best weapon functionality
	 * this value only has meaning on clients
	 */
	UPROPERTY(EditAnywhere, Config, Transient, BlueprintReadOnly, Category = "Selection")
	float AutoSwitchPriority;

	/** Deactivate any owner spawn protection */
	virtual void DeactivateSpawnProtection();

	/** whether this weapon stays around by default when someone picks it up (i.e. multiple people can pick up from the same spot without waiting for respawn time) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	bool bWeaponStay;

	/** Base offset of first person mesh, cached from offset set up in blueprint. */
	UPROPERTY()
	FVector FirstPMeshOffset;

	/** Base relative rotation of first person mesh, cached from offset set up in blueprint. */
	UPROPERTY()
	FRotator FirstPMeshRotation;

	/** Scaling for 1st person weapon bob */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBob")
	float WeaponBobScaling;

	/** Scaling for 1st person firing view kickback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBob")
	float FiringViewKickback;

	virtual void UpdateViewBob(float DeltaTime);

	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void RegisterAllComponents() override
	{
		// don't register in game by default for perf, we'll manually call Super from AttachToOwner()
		if (GetWorld()->WorldType == EWorldType::Editor || GetWorld()->WorldType == EWorldType::Preview)
		{
			Super::RegisterAllComponents();
		}
		else
		{
			RootComponent = NULL; // this was set for the editor view, but we don't want it
		}
	}

	virtual UMeshComponent* GetPickupMeshTemplate_Implementation(FVector& OverrideScale) const override;

	virtual void GotoState(class UUTWeaponState* NewState);
	/** notification of state change (CurrentState is new state)
	 * if a state change triggers another state change (i.e. within BeginState()/EndState()) this function will only be called once, when CurrentState is the final state
	 */
	virtual void StateChanged()
	{}

	/** firing entry point */
	virtual void StartFire(uint8 FireModeNum);
	virtual void StopFire(uint8 FireModeNum);

	/** Tell server fire button was pressed.  bClientFired is true if client actually fired weapon. */
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerStartFire(uint8 FireModeNum, bool bClientFired);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerStopFire(uint8 FireModeNum);

	virtual bool BeginFiringSequence(uint8 FireModeNum, bool bClientFired);
	virtual void EndFiringSequence(uint8 FireModeNum);

	/** Returns true if weapon will fire a shot this frame - used for network synchronization */
	virtual bool WillSpawnShot(float DeltaTime);

	/** Returns true if weapon can fire again (fire button is pressed, weapon is held, has ammo, etc.). */
	virtual bool CanFireAgain();

	/** hook when the firing state starts; called on both client and server */
	UFUNCTION(BlueprintNativeEvent)
	void OnStartedFiring();

	/** hook for the return to active state (was firing, refire timer expired, trigger released and/or out of ammo)  */
	UFUNCTION(BlueprintNativeEvent)
	void OnStoppedFiring();

	/** Return true and  trigger effects if should continue firing, otherwise sends weapon to its active state */
	virtual bool HandleContinuedFiring();

	/** hook for when the weapon has fired, the refire delay passes, and the user still wants to fire (trigger still down) so the firing loop will repeat */
	UFUNCTION(BlueprintNativeEvent)
	void OnContinuedFiring();

	/** blueprint hook for pressing one fire mode while another is currently firing (e.g. hold alt, press primary)
	 * CurrentFireMode == current, OtherFireMode == one just pressed
	 */
	UFUNCTION(BlueprintNativeEvent)
	void OnMultiPress(uint8 OtherFireMode);

	/** activates the weapon as part of a weapon switch
	 * (this weapon has already been set to Pawn->Weapon)
	 * @param OverflowTime - overflow from timer of previous weapon PutDown() due to tick delta
	 */
	virtual void BringUp(float OverflowTime = 0.0f);
	/** puts the weapon away as part of a weapon switch
	 * return false to prevent weapon switch (must keep this weapon equipped)
	 */
	virtual bool PutDown();

	/**Hook to do effects when the weapon is raised*/
	UFUNCTION(BlueprintImplementableEvent)
	void OnBringUp();

	/** allows blueprint to prevent the weapon from being switched away from */
	UFUNCTION(BlueprintImplementableEvent)
	bool eventPreventPutDown();

	/** attach the visuals to Owner's first person view */
	UFUNCTION(BlueprintNativeEvent)
	void AttachToOwner();

	/** detach the visuals from the Owner's first person view */
	UFUNCTION(BlueprintNativeEvent)
	void DetachFromOwner();

	/** return number of fire modes */
	virtual uint8 GetNumFireModes() const
	{
		return FMath::Min3(255, FiringState.Num(), FireInterval.Num());
	}

	/** returns if the specified fire mode is a charged mode - that is, if the trigger is held firing will be delayed and the effect will improve in some way */
	virtual bool IsChargedFireMode(uint8 TestMode) const;

	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) override;
	virtual void ClientGivenTo_Internal(bool bAutoActivate) override;

	virtual void Removed() override;
	virtual void ClientRemoved_Implementation() override;

	/** fires a shot and consumes ammo */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void FireShot();
	/** blueprint override for firing
	 * NOTE: do an authority check before spawning projectiles, etc as this function is called on both sides
	 */
	UFUNCTION(BlueprintImplementableEvent)
	bool FireShotOverride();

	/** returns montage to play on the weapon for the specified firing mode */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual UAnimMontage* GetFiringAnim(uint8 FireMode) const;
	/** play firing effects not associated with the shot's results (e.g. muzzle flash but generally NOT emitter to target) */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void PlayFiringEffects();
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void StopFiringEffects();

	/** blueprint hook to modify spawned instance of FireEffect (e.g. tracer or beam) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void ModifyFireEffect(UParticleSystemComponent* Effect);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void GetImpactSpawnPosition(const FVector& TargetLoc, FVector& SpawnLocation, FRotator& SpawnRotation);

	/** If true, don't spawn impact effect.  Used for hitscan hits, skips by default for pawn and projectile hits. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual bool CancelImpactEffect(const FHitResult& ImpactHit);

	/** play effects associated with the shot's impact given the impact point
	 * called only if FlashLocation has been set (instant hit weapon)
	 * Call GetImpactSpawnPosition() to set SpawnLocation and SpawnRotation
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void PlayImpactEffects(const FVector& TargetLoc, uint8 FireMode, const FVector& SpawnLocation, const FRotator& SpawnRotation);

	/** shared code between UTWeapon and UTWeaponAttachment to refine replicated FlashLocation into impact effect transform via trace */
	static FHitResult GetImpactEffectHit(APawn* Shooter, const FVector& StartLoc, const FVector& TargetLoc);

	/** return start point for weapons fire */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	virtual FVector GetFireStartLoc(uint8 FireMode = 255);
	/** return base fire direction for weapons fire (i.e. direction player's weapon is pointing) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	virtual FRotator GetBaseFireRotation();
	/** return adjusted fire rotation after accounting for spread, aim help, and any other secondary factors affecting aim direction (may include randomized components) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintPure, Category = "Weapon")
	FRotator GetAdjustedAim(FVector StartFireLoc);

	/** if owned by a human, set AUTPlayerController::LastShotTargetGuess to closest target to player's aim */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = AI)
	void GuessPlayerTarget(const FVector& StartFireLoc, const FVector& FireDir);

	/** add (or remove via negative number) the ammo held by the weapon */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	virtual void AddAmmo(int32 Amount);

	/** use up AmmoCost units of ammo for the current fire mode
	 * also handles triggering auto weapon switch if out of ammo
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	virtual void ConsumeAmmo(uint8 FireModeNum);

	virtual void FireInstantHit(bool bDealDamage = true, FHitResult* OutHit = NULL);

	UFUNCTION(BlueprintCallable, Category = Firing)
	void K2_FireInstantHit(bool bDealDamage, FHitResult& OutHit);

	/** Handles rewind functionality for net games with ping prediction */
	virtual void HitScanTrace(const FVector& StartLocation, const FVector& EndTrace, FHitResult& Hit, float PredictionTime);

	UFUNCTION(BlueprintCallable, Category = Firing)
	virtual AUTProjectile* FireProjectile();

	/** Spawn a projectile on both server and owning client, and forward predict it by 1/2 ping on server. */
	virtual AUTProjectile* SpawnNetPredictedProjectile(TSubclassOf<AUTProjectile> ProjectileClass, FVector SpawnLocation, FRotator SpawnRotation);

	/** returns whether we can meet AmmoCost for the given fire mode */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual bool HasAmmo(uint8 FireModeNum);

	/** returns whether we have ammo for any fire mode */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual bool HasAnyAmmo();

	/** get interval between shots, including any fire rate modifiers */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual float GetRefireTime(uint8 FireModeNum);

	inline uint8 GetCurrentFireMode()
	{
		return CurrentFireMode;
	}

	inline void GotoActiveState()
	{
		GotoState(ActiveState);
	}

	virtual void GotoFireMode(uint8 NewFireMode);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool IsFiring() const;

	virtual bool StackPickup_Implementation(AUTInventory* ContainedInv) override;

	/** update any timers and such due to a weapon timing change; for example, a powerup that changes firing speed */
	virtual void UpdateTiming();

	virtual void Tick(float DeltaTime) override;

	virtual void Destroyed() override;

	/** we added an editor tool to allow the user to set the MuzzleFlash entries to a component created in the blueprint components view,
	 * but the resulting instances won't be automatically set...
	 * so we need to manually hook it all up
	 * static so we can share with UTWeaponAttachment
	 */
	static void InstanceMuzzleFlashArray(AActor* Weap, TArray<UParticleSystemComponent*>& MFArray);

	inline UUTWeaponState* GetCurrentState()
	{
		return CurrentState;
	}

	/** called on clients only when the local owner got a kill while holding this weapon
	 * NOTE: this weapon didn't necessarily cause the kill (previously fired projectile, etc), if you care check the damagetype
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void NotifyKillWhileHolding(TSubclassOf<UDamageType> DmgType);

	/** returns scaling of head for headshot effects
	 * NOTE: not used by base weapon implementation; stub is here for subclasses and firemodes that use it to avoid needing to cast to a specific weapon type
	 */
	virtual float GetHeadshotScale() const
	{
		return 0.0f;
	}

	/** Return true if needs HUD ammo display widget drawn. */
	virtual bool NeedsAmmoDisplay() const;

	/** returns crosshair color taking into account user settings, red flash on hit, etc */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	FLinearColor GetCrosshairColor(UUTHUDWidget* WeaponHudWidget) const;

	/** The player state of the player currently under the crosshair */
	UPROPERTY()
	AUTPlayerState* TargetPlayerState;

	/** The time this player was last seen under the crosshaiar */
	float TargetLastSeenTime;

	/** returns whether we should draw the friendly fire indicator on the crosshair */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	virtual bool ShouldDrawFFIndicator(APlayerController* Viewer, AUTPlayerState *& HitPlayerState ) const;

	/** Returns desired crosshair scale (affected by recent pickups */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	virtual float GetCrosshairScale(class AUTHUD* HUD);

	UFUNCTION(BlueprintNativeEvent)
	void DrawWeaponCrosshair(UUTHUDWidget* WeaponHudWidget, float RenderDelta);

	UFUNCTION()
	void UpdateCrosshairTarget(AUTPlayerState* NewCrosshairTarget, UUTHUDWidget* WeaponHudWidget, float RenderDelta);

	/** helper for shared overlay code between UTWeapon and UTWeaponAttachment
	 * NOTE: can called on default object!
	 */
	virtual void UpdateOverlaysShared(AActor* WeaponActor, AUTCharacter* InOwner, USkeletalMeshComponent* InMesh, USkeletalMeshComponent*& InOverlayMesh) const;
	/** read WeaponOverlayFlags from owner and apply the appropriate overlay material (if any) */
	virtual void UpdateOverlays();

	/** set main skin override for the weapon, NULL to restore to default */
	virtual void SetSkin(UMaterialInterface* NewSkin);

	//*********
	// Rotation Lag/Lead

	/** Previous frame's weapon rotation */
	UPROPERTY()
	FRotator LastRotation;

	/** Saved values used for lagging weapon rotation */
	UPROPERTY()
	float	OldRotDiff[2];
	UPROPERTY()
	float	OldLeadMag[2];
	UPROPERTY()
	float	OldMaxDiff[2];

	/** How fast Weapon Rotation offsets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Rotation")
	float	RotChgSpeed; 

	/** How fast Weapon Rotation returns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Rotation")
	float	ReturnChgSpeed;

	/** Max Weapon Rotation Yaw offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Rotation")
	float	MaxYawLag;

	/** Max Weapon Rotation Pitch offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Rotation")
	float	MaxPitchLag;

	/** @return whether the weapon's rotation is allowed to lag behind the holder's rotation */
	virtual bool ShouldLagRot();

	/** Lag a component of weapon rotation behind player's rotation. */
	virtual float LagWeaponRotation(float NewValue, float LastValue, float DeltaTime, float MaxDiff, int32 Index);

	/** called when initially attaching the first person weapon and when a camera viewing this player changes the weapon hand setting */
	virtual void UpdateWeaponHand();

	/** get weapon hand from the owning playercontroller, or spectating playercontroller if it's a client and a nonlocal player */
	EWeaponHand GetWeaponHand() const;

	/** Begin unequipping this weapon */
	virtual void UnEquip();

	virtual void GotoEquippingState(float OverflowTime);
	
	/** informational function that returns the damage radius that a given fire mode has (used by e.g. bots) */
	UFUNCTION(BlueprintNativeEvent, Category = AI)
	float GetDamageRadius(uint8 TestMode) const;

	virtual float BotDesireability_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const;
	virtual float DetourWeight_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const;
	/** base weapon selection rating for AI
	 * this is often used to determine if the AI has a good enough weapon to not pursue further pickups,
	 * since GetAIRating() will fluctuate wildly depending on the combat scenario
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	float BaseAISelectRating;
	/** AI switches to the weapon that returns the highest rating */
	UFUNCTION(BlueprintNativeEvent, Category = AI)
	float GetAISelectRating();

	/** return a value from -1 to 1 for suggested method of attack for AI when holding this weapon, where < 0 indicates back off and fire from afar while > 0 indicates AI should advance/charge */
	UFUNCTION(BlueprintNativeEvent, Category = AI)
	float SuggestAttackStyle();
	/** return a value from -1 to 1 for suggested method of defense for AI when fighting a player with this weapon, where < 0 indicates back off and fire from afar while > 0 indicates AI should advance/charge */
	UFUNCTION(BlueprintNativeEvent, Category = AI)
	float SuggestDefenseStyle();

	/** called by the AI to ask if a weapon attack is being prepared; for example loading rockets or waiting for a shock ball to combo
	 * the AI uses this to know it shouldn't move around too much and should focus on its current target to avoid messing up the shot
	 */
	UFUNCTION(BlueprintNativeEvent, Category = AI)
	bool IsPreparingAttack();
	/** called by the AI to ask if it should avoid firing even if the weapon can currently hit its target (e.g. setting up a combo) */
	UFUNCTION(BlueprintNativeEvent, Category = AI)
	bool ShouldAIDelayFiring();

	/** returns whether this weapon has a viable attack against Target
	 * this function should not consider Owner's view rotation
	 * @param Target - Target actor
	 * @param TargetLoc - Target location, not guaranteed to be Target's true location (AI may pass a predicted or guess location)
	 * @param bDirectOnly - if true, only return success if weapon can directly hit Target from its current location (i.e. no need to wait for owner or target to move, no bounce shots, etc)
	 * @param bPreferCurrentMode - if true, only change BestFireMode if current value can't attack target but another mode can
	 * @param BestFireMode (in/out) - (in) current fire mode bot is set to use; (out) the fire mode that would be best to use for the attack
	 * @param OptimalTargetLoc (out) - best position to shoot at to hit TargetLoc (generally TargetLoc unless weapon has an indirect or special attack that doesn't require pointing at the target)
	 */
	UFUNCTION(BlueprintNativeEvent, Category = AI)
	bool CanAttack(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, UPARAM(ref) uint8& BestFireMode, UPARAM(ref) FVector& OptimalTargetLoc);

	/** convenience redirect if the out params are not needed (just checking if firing is a good idea)
	 * would prefer to use pointer params but Blueprints don't support that
	 */
	inline bool CanAttack(AActor* Target, const FVector& TargetLoc, bool bDirectOnly)
	{
		uint8 UnusedFireMode;
		FVector UnusedOptimalLoc;
		return CanAttack(Target, TargetLoc, bDirectOnly, false, UnusedFireMode, UnusedOptimalLoc);
	}

	/** called by AI to try an assisted jump (e.g. impact jump or rocket jump)
	 * return true if an action was performed and the bot can continue along its path
	 */
	virtual bool DoAssistedJump()
	{
		return false;
	}

	/** returns the meshes used to represent this weapon in first person, for example so they can be hidden when viewing in 3p
	 * weapons that have additional relevant meshes (hands, dual wield, etc) should override to return those additional components
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Mesh)
	TArray<UMeshComponent*> Get1PMeshes() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	UUTWeaponState* CurrentState;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	uint8 CurrentFireMode;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	UUTWeaponState* ActiveState;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	class UUTWeaponStateEquipping* EquippingState;
	UPROPERTY(Instanced, BlueprintReadOnly,  Category = "States")
	UUTWeaponState* UnequippingStateDefault;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	UUTWeaponState* UnequippingState;

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "States")
	UUTWeaponState* InactiveState;

	/** overlay mesh for overlay effects */
	UPROPERTY()
	USkeletalMeshComponent* OverlayMesh;

public:
	float WeaponBarScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Weapon)
	FLinearColor IconColor;

	// The UV coords for this weapon when displaying it on the weapon bar
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FTextureUVs WeaponBarSelectedUVs;

	// The UV coords for this weapon when displaying it on the weapon bar
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FTextureUVs WeaponBarInactiveUVs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
		FName KillStatsName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
		FName AltKillStatsName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
		FName DeathStatsName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
		FName AltDeathStatsName;

	virtual int32 GetWeaponKillStats(AUTPlayerState * PS) const;
	virtual int32 GetWeaponDeathStats(AUTPlayerState * PS) const;

	// TEMP for testing 1p offsets
	UFUNCTION(exec)
	void TestWeaponLoc(float X, float Y, float Z);
	UFUNCTION(exec)
	void TestWeaponRot(float Pitch, float Yaw, float Roll = 0.0f);
	UFUNCTION(exec)
	void TestWeaponScale(float X, float Y, float Z);

	/** blueprint hook to modify team color materials */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void NotifyTeamChanged();

	UFUNCTION(BlueprintNativeEvent, Category = Weapon)
	void FiringInfoUpdated(uint8 InFireMode, uint8 FlashCount, FVector InFlashLocation);
	UFUNCTION(BlueprintNativeEvent, Category = Weapon)
	void FiringExtraUpdated(uint8 NewFlashExtra, uint8 InFireMode);
};