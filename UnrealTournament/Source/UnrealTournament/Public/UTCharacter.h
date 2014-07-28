// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacter.generated.h"

USTRUCT(BlueprintType)
struct FTakeHitInfo
{
	GENERATED_USTRUCT_BODY()

	/** the amount of damage */
	UPROPERTY(BlueprintReadWrite, Category = TakeHitInfo)
	int32 Damage;
	/** the location of the hit (relative to Pawn center) */
	UPROPERTY(BlueprintReadWrite, Category = TakeHitInfo)
	FVector_NetQuantize RelHitLocation;
	/** how much momentum was imparted */
	UPROPERTY(BlueprintReadWrite, Category = TakeHitInfo)
	FVector_NetQuantize Momentum;
	/** the damage type we were hit with */
	UPROPERTY(BlueprintReadWrite, Category = TakeHitInfo)
	TSubclassOf<UDamageType> DamageType;
};

/** ammo counter */
USTRUCT(BlueprintType)
struct FStoredAmmo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Ammo)
	TSubclassOf<class AUTWeapon> Type;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Ammo)
	int32 Amount;
};

UCLASS(config=Game, collapsecategories, hidecategories=(Clothing,Lighting,AutoExposure,LensFlares,AmbientOcclusion,DepthOfField,MotionBlur,Misc,ScreenSpaceReflections,Bloom,SceneColor,Film,AmbientCubemap,AgentPhysics,Attachment,Avoidance,PlanarMovement,AI,Replication,Input,Actor,Tags,GlobalIllumination))
class AUTCharacter : public ACharacter, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	TSubobjectPtr<class USkeletalMeshComponent> FirstPersonMesh;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	TSubobjectPtr<class UCameraComponent> CharacterCameraComponent;

	/** Cached UTCharacterMovement casted CharacterMovement */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly)
	class UUTCharacterMovement* UTCharacterMovement;

	/** counters of ammo for which the pawn doesn't yet have the corresponding weapon in its inventory */
	UPROPERTY()
	TArray<FStoredAmmo> SavedAmmo;

	UFUNCTION(BlueprintCallable, Category = Pawn)
	virtual void AddAmmo(const FStoredAmmo& AmmoToAdd);

	/** returns whether the character (via SavedAmmo, active weapon, or both) has the maximum allowed ammo for the passed in weapon */
	UFUNCTION(BlueprintCallable, Category = Pawn)
	virtual bool HasMaxAmmo(TSubclassOf<AUTWeapon> Type);

	// Cheat, only works if called server side
	void AllAmmo();

	// Cheat, only works if called server side
	void UnlimitedAmmo();

	UPROPERTY(replicated)
	bool bUnlimitedAmmo;

	inline class AUTInventory* GetInventory()
	{
		return InventoryList;
	}

	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void AddInventory(AUTInventory* InvToAdd, bool bAutoActivate);

	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void RemoveInventory(AUTInventory* InvToRemove);

	/** find an inventory item of a specified type */
	UFUNCTION(BlueprintCallable, Category = Pawn)
	virtual AUTInventory* K2_FindInventoryType(TSubclassOf<AUTInventory> Type, bool bExactClass = false);

	template<typename InvClass>
	inline InvClass* FindInventoryType(TSubclassOf<InvClass> Type, bool bExactClass = false)
	{
		InvClass* Result = (InvClass*)K2_FindInventoryType(Type, bExactClass);
		checkSlow(Result == NULL || Result->IsA(InvClass::StaticClass()));
		return Result;
	}

	/** toss an inventory item in the direction the player is facing
	 * (the inventory must have a pickup defined)
	 * ExtraVelocity is in the reference frame of the character (X is forward)
	 */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void TossInventory(AUTInventory* InvToToss, FVector ExtraVelocity = FVector::ZeroVector);

	/** discards (generally destroys) all inventory items */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void DiscardAllInventory();

	/** call to propagate a named character event (jumping, firing, etc) to all inventory items with bCallOwnerEvent = true */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void InventoryEvent(FName EventName);

	/** switches weapons; handles client/server sync, safe to call on either side */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void SwitchWeapon(AUTWeapon* NewWeapon);

	inline bool IsPendingFire(uint8 FireMode) const
	{
		return (FireMode < PendingFire.Num() && PendingFire[FireMode] != 0);
	}
	/** blueprint accessor to what firemodes the player currently has active */
	UFUNCTION(BlueprintPure, Category = Weapon)
	bool IsTriggerDown(uint8 FireMode);
	/** sets the pending fire flag; generally should be called by whatever weapon processes the firing command, unless it's an explicit single shot */
	inline void SetPendingFire(uint8 FireMode, bool bNowFiring)
	{
		if (PendingFire.Num() < FireMode + 1)
		{
			PendingFire.SetNumZeroed(FireMode + 1);
		}
		PendingFire[FireMode] = bNowFiring ? 1 : 0;
	}

	inline AUTWeapon* GetWeapon() const
	{
		return Weapon;
	}
	inline TSubclassOf<AUTWeapon> GetWeaponClass() const
	{
		// debug check to make sure this matches as expected
		checkSlow(GetNetMode() == NM_Client || (Weapon == NULL ? WeaponClass == NULL : ((UObject*)Weapon)->GetClass() == WeaponClass));
		
		return WeaponClass;
	}
	inline AUTWeapon* GetPendingWeapon() const
	{
		return PendingWeapon;
	}

	bool IsInInventory(const AUTInventory* TestInv) const;

	/** called by weapon being put down when it has finished being unequipped. Transition PendingWeapon to Weapon and bring it up */
	virtual void WeaponChanged();

	/** called when the client's current weapon has been invalidated (removed from inventory, etc) */
	UFUNCTION(Client, Reliable)
	void ClientWeaponLost(AUTWeapon* LostWeapon);

	/** replicated weapon firing info */
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = FiringInfoUpdated, Category = "Weapon")
	uint8 FlashCount;
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Weapon")
	uint8 FireMode;
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = FiringInfoUpdated, Category = "Weapon")
	FVector_NetQuantize FlashLocation;

	/** set info for one instance of firing and plays firing effects; assumed to be a valid shot - call ClearFiringInfo() if the weapon has stopped firing
	 * if a location is not needed (projectile) call IncrementFlashCount() instead
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void SetFlashLocation(const FVector& InFlashLoc, uint8 InFireMode);

	/** set info for one instance of firing and plays firing effects; assumed to be a valid shot - call ClearFiringInfo() if the weapon has stopped firing
	* if a location is needed (instant hit, beam, etc) call SetFlashLocation() instead
	*/
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void IncrementFlashCount(uint8 InFireMode);

	/** clears firing variables; i.e. because the weapon has stopped firing */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void ClearFiringInfo();

	/** called when firing variables are updated to trigger/stop effects */
	UFUNCTION()
	virtual void FiringInfoUpdated();

	UPROPERTY(BlueprintReadWrite, Category = Pawn, Replicated)
	int32 Health;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Pawn)
	int32 HealthMax;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Pawn)
	int32 SuperHealthMax;

	/** head bone/socket for headshots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Pawn)
	FName HeadBone;
	/** head Z offset from head bone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Pawn)
	float HeadHeight;
	/** radius around head location that counts as headshot at 1.0 head scaling */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Pawn)
	float HeadRadius;
	/** head scale factor (generally for use at runtime) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = HeadScaleUpdated, Category = Pawn)
	float HeadScale;

	/** multiplier to damage caused by this Pawn */
	UPROPERTY(BlueprintReadWrite, Replicated, Category = Pawn)
	float DamageScaling;
	
	/** accessors to FireRateMultiplier */
	UFUNCTION(BlueprintCallable, Category = Pawn)
	float GetFireRateMultiplier();
	UFUNCTION(BlueprintCallable, Category = Pawn)
	void SetFireRateMultiplier(float InMult);
	UFUNCTION()
	void FireRateChanged();

	UPROPERTY(BlueprintReadWrite, Category = Pawn, Replicated, ReplicatedUsing=PlayTakeHitEffects)
	FTakeHitInfo LastTakeHitInfo;

	/** time of last SetLastTakeHitInfo() - authority only */
	UPROPERTY(BlueprintReadOnly, Category = Pawn)
	float LastTakeHitTime;

	/** indicates character is (mostly) invisible so AI only sees at short range, homing effects can't target the character, etc */
	UPROPERTY(BlueprintReadWrite, Category = Pawn)
	bool bInvisible;

	/** whether spawn protection may potentially be applied (still must meet time since spawn check in UTGameMode)
	 * set to false after firing weapon or any other action that is considered offensive
	 */
	UPROPERTY(BlueprintReadOnly, Category = Pawn)
	bool bSpawnProtectionEligible;

	/** set temporarily during client reception of replicated properties because replicated position and switches to ragdoll may be processed out of the desired order 
	 * when set, OnRep_ReplicatedMovement() will be called after switching to ragdoll
	 */
	bool bDeferredReplicatedMovement;

	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void Destroyed() override;

	virtual void PossessedBy(AController* NewController) override;
	virtual void Restart() override;

	virtual bool ShouldTakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) const override
	{
		return bTearOff || Super::ShouldTakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	}

	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** checks for a head shot - called by weapons with head shot bonuses
	* returns true if it's a head shot, false if a miss or if some armor effect prevents head shots
	* if bConsumeArmor is true, the first item that prevents an otherwise valid head shot will be consumed
	*/
	virtual bool IsHeadShot(FVector HitLocation, FVector ShotDirection, float WeaponHeadScaling, bool bConsumeArmor);

	UFUNCTION(BlueprintCallable, Category = Pawn)
	void SetHeadScale(float NewHeadScale);
	
	/** apply HeadScale to mesh */
	UFUNCTION()
	virtual void HeadScaleUpdated();

	/** sends notification to any other server-side Actors (controller, etc) that need to know about being hit */
	virtual void NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, const FDamageEvent& DamageEvent);

	/** Set LastTakeHitInfo from a damage event and call PlayTakeHitEffects() */
	virtual void SetLastTakeHitInfo(int32 Damage, const FVector& Momentum, const FDamageEvent& DamageEvent);

	/** TEMP blood effect until we have a better hit effects system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	UParticleSystem* BloodEffect;

	/** plays clientside hit effects using the data previously stored in LastTakeHitInfo */
	UFUNCTION(BlueprintNativeEvent, BlueprintCosmetic)
	void PlayTakeHitEffects();

	/** called when we die (generally, by running out of health)
	 *  SERVER ONLY - do not do visual effects here!
	 * return true if we can die, false if immortal (gametype effect, powerup, mutator, etc)
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Pawn)
	virtual bool Died(AController* EventInstigator, const FDamageEvent& DamageEvent);

	/** plays death effects; use LastTakeHitInfo to do damage-specific death effects */
	virtual void PlayDying();
	virtual void TornOff() override
	{
		PlayDying();
	}

	virtual void DeactivateSpawnProtection();

	virtual void AddDefaultInventory(TArray<TSubclassOf<AUTInventory>> DefaultInventoryToAdd);

	UFUNCTION(BlueprintCallable, Category = Pawn)
	bool IsDead();

	/** weapon firing */
	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void StartFire(uint8 FireModeNum);

	UFUNCTION(BlueprintCallable, Category = "Pawn")
	virtual void StopFire(uint8 FireModeNum);

	/** Return true if character is currently able to dodge. */
	virtual bool CanDodge() const;

	/** Dodge requested by controller, return whether dodge occurred. */
	virtual bool Dodge(FVector DodgeDir, FVector DodgeCross);

	/** Dodge just occured in dodge dir, play any sounds/effects desired.
	 * called on server and owning client
	 */
	UFUNCTION(BlueprintNativeEvent)
	void OnDodge(const FVector &DodgeDir);

	/** Landing assist just occurred */
	UFUNCTION(BlueprintImplementableEvent)
	virtual void OnLandingAssist();

	/** Blueprint override for dodge handling. Rteturn true to skip default dodge in C++. */
	UFUNCTION(BlueprintImplementableEvent)
	bool DodgeOverride(const FVector &DodgeDir, const FVector &DodgeCross);

	virtual bool CanJumpInternal_Implementation() const override;

	virtual void CheckJumpInput(float DeltaTime) override;

	virtual void NotifyJumpApex();

	/** Handles moving forward/backward */
	virtual void MoveForward(float Val);

	/** Handles strafing movement, left and right */
	virtual void MoveRight(float Val);

	/** Handles up and down when swimming or flying */
	virtual void MoveUp(float Val);

	virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker) override;
	virtual void OnRep_ReplicatedMovement() override;

	virtual void SetBase(UPrimitiveComponent* NewBase, bool bNotifyActor=true) override;

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	/** Also call UTCharacterMovement ClearJumpInput() */
	virtual void ClearJumpInput() override;

	virtual void MoveBlockedBy(const FHitResult& Impact) override;

	/** sets replicated ambient (looping) sound on this Pawn
	 * only one ambient sound can be set at a time
	 * pass bClear with a valid NewAmbientSound to remove only if NewAmbientSound == CurrentAmbientSound
	 */
	UFUNCTION(BlueprintCallable, Category = Audio)
	virtual void SetAmbientSound(USoundBase* NewAmbientSound, bool bClear = false);

	UFUNCTION()
	void AmbientSoundUpdated();

	UFUNCTION(BlueprintPure, Category = PlayerController)
	virtual APlayerCameraManager* GetPlayerCameraManager();

	/** plays a footstep effect; called via animation when anims are active (in vis range and not server), otherwise on interval via Tick() */
	UFUNCTION(BlueprintCallable, Category = Effects)
	virtual void PlayFootstep(uint8 FootNum);

	/** play jumping sound/effects; should be called on server and owning client */
	UFUNCTION(BlueprintCallable, Category = Effects)
	virtual void PlayJump();

	/** Landing at faster than this velocity results in damage (note: use positive number) */
	UPROPERTY(Category = "Falling Damage", EditAnywhere, BlueprintReadWrite)
	float MaxSafeFallSpeed;
	/** amount of falling damage dealt if the player's fall speed is double MaxSafeFallSpeed (scaled linearly from there) */
	UPROPERTY(Category = "Falling Damage", EditAnywhere, BlueprintReadWrite)
	float FallingDamageFactor;
	/** amount of damage dealt to other characters we land on per 100 units of speed */
	UPROPERTY(Category = "Falling Damage", EditAnywhere, BlueprintReadWrite)
	float CrushingDamageFactor;

	UFUNCTION(BlueprintCallable, Category = Pawn)
	virtual void TakeFallingDamage(const FHitResult& Hit);

	virtual void Landed(const FHitResult& Hit) override;

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
	USoundBase* FootstepSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
	USoundBase* LandingSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
	USoundBase* JumpSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
	USoundBase* DodgeSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
	USoundBase* PainSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
	USoundBase* WallHitSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
	USoundBase* DodgeRollSound;

	UPROPERTY(BlueprintReadWrite, Category = Sounds)
	float LastPainSoundTime;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
	float MinPainSoundInterval;

	UPROPERTY(BlueprintReadWrite, Category = Sounds)
	float LastWallHitSoundTime;

	// Controls if we want to see the first or third person meshes
	void SetMeshVisibility(bool bThirdPersonView);

	/** sets character overlay material; material must be added to the UTGameState's OverlayMaterials at level startup to work correctly (for replication reasons)
	 * multiple overlays can be active at once, but only one will be displayed at a time
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Effects)
	virtual void SetCharacterOverlay(UMaterialInterface* NewOverlay, bool bEnabled);

	/** uses CharOverlayFlags to apply the desired overlay material (if any) to OverlayMesh */
	UFUNCTION()
	virtual void UpdateCharOverlays();

	/** sets weapon overlay material; material must be added to the UTGameState's OverlayMaterials at level startup to work correctly (for replication reasons)
	 * multiple overlays can be active at once, but the default in the weapon code is to only display one at a time
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Effects)
	virtual void SetWeaponOverlay(UMaterialInterface* NewOverlay, bool bEnabled);

	/** uses WeaponOverlayFlags to apply the desired overlay material (if any) to OverlayMesh */
	UFUNCTION()
	virtual void UpdateWeaponOverlays();

	inline int16 GetCharOverlayFlags()
	{
		return CharOverlayFlags;
	}
	inline int16 GetWeaponOverlayFlags()
	{
		return WeaponOverlayFlags;
	}

	/** sets full body material override
	 * only one at a time is allowed
	 * pass NULL to restore default skin
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Effects)
	virtual void SetSkin(UMaterialInterface* NewSkin);
	inline UMaterialInterface* GetSkin()
	{
		return ReplicatedBodyMaterial;
	}

	/** apply skin in ReplicatedBodyMaterial or restore to default if it's NULL */
	UFUNCTION()
	virtual void UpdateSkin();

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual uint8 GetTeamNum() const;

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual FLinearColor GetTeamColor() const;

	virtual void OnRep_PlayerState() override;
	virtual void NotifyTeamChanged();

	virtual void PlayerChangedTeam();
	virtual void PlayerSuicide();

	//--------------------------
	// Weapon bob and eye offset

	/** Current 1st person weapon deflection due to running bob. */
	UPROPERTY(BlueprintReadWrite, Category = WeaponBob)
	FVector CurrentWeaponBob;

	/** Max 1st person weapon bob deflection with axes based on player view */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	FVector WeaponBobMagnitude;

	/** Z deflection of first person weapon when player jumps */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	FVector WeaponJumpBob;

	/** deflection of first person weapon when player dodges */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	FVector WeaponDodgeBob;

	/** Z deflection of first person weapon when player lands */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	FVector WeaponLandBob;

	/** Desired 1st person weapon deflection due to jumping. */
	UPROPERTY(BlueprintReadWrite, Category = WeaponBob)
	FVector DesiredJumpBob;

	/* Current jump bob (interpolating to DesiredJumpBob)*/
	UPROPERTY(BlueprintReadWrite, Category = WeaponBob)
	FVector CurrentJumpBob;

	/** Time used for weapon bob sinusoids, reset on landing. */
	UPROPERTY(BlueprintReadWrite, Category = WeaponBob)
	float BobTime;

	/** Rate of weapon bob when standing still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float WeaponBreathingBobRate;

	/** Rate of weapon bob when running. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float WeaponRunningBobRate;

	/** How fast to interpolate to Jump/Land bob offset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float WeaponJumpBobInterpRate;

	/** How fast to decay out Land bob offset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float WeaponLandBobDecayRate;

	/** Get Max eye offset land bob deflection at landing velocity Z of FullEyeOffsetLandBobVelZ+EyeOffsetLandBobThreshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float WeaponDirChangeDeflection;

	/** Current Eye position offset from base view position - interpolates toward TargetEyeOffset. */
	UPROPERTY(BlueprintReadWrite, Category = WeaponBob)
	FVector EyeOffset;

	/** Eyeoffset due to crouching transition (not scaled). */
	UPROPERTY(BlueprintReadWrite, Category = WeaponBob)
	FVector CrouchEyeOffset;

	/** Target Eye position offset from base view position. */
	UPROPERTY(BlueprintReadWrite, Category = WeaponBob)
	FVector TargetEyeOffset;

	/** How fast EyeOffset interpolates to TargetEyeOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float EyeOffsetInterpRate;

	/** How fast CrouchEyeOffset interpolates to 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float CrouchEyeOffsetInterpRate;

	/** How fast TargetEyeOffset decays. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float EyeOffsetDecayRate;

	/** Jump target view bob magnitude. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float EyeOffsetJumpBob;

	/** Jump Landing target view bob magnitude. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float EyeOffsetLandBob;

	/** Jump Landing target view bob Velocity threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float EyeOffsetLandBobThreshold;

	/** Jump Landing target weapon bob Velocity threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float WeaponLandBobThreshold;

	/** Get Max weapon land bob deflection at landing velocity Z of FullWeaponLandBobVelZ+WeaponLandBobThreshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float FullWeaponLandBobVelZ;

	/** Get Max eye offset land bob deflection at landing velocity Z of FullEyeOffsetLandBobVelZ+EyeOffsetLandBobThreshold */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = WeaponBob)
	float FullEyeOffsetLandBobVelZ;

	/** Get Max weapon land bob deflection at landing velocity Z of FullWeaponLandBobVelZ+WeaponLandBobThreshold */
	UPROPERTY()
	float DefaultBaseEyeHeight;

	virtual void RecalculateBaseEyeHeight() override;

	/** Returns offset to add to first person mesh for weapon bob. */
	FVector GetWeaponBobOffset(float DeltaTime, AUTWeapon* MyWeapon);

	virtual FVector GetPawnViewLocation() const override;

	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

	virtual void OnEndCrouch(float HeightAdjust, float ScaledHeightAdjust) override;

	virtual void OnStartCrouch(float HeightAdjust, float ScaledHeightAdjust) override;

	//--------------------------

	/**
	 *	@returns true if this 
	 **/
	virtual bool CanPickupObject(AUTCarriedObject* PendingObject);

	
	/**
	 *	@return the current object carried by this pawn
	 **/
	UFUNCTION()
	virtual AUTCarriedObject* GetCarriedObject();


protected:

	/** multiplier to firing speed */
	UPROPERTY(Replicated, ReplicatedUsing=FireRateChanged)
	float FireRateMultiplier;

	/** hook to modify damage taken by this Pawn */
	UFUNCTION(BlueprintNativeEvent)
	void ModifyDamageTaken(int32& Damage, FVector& Momentum, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser);
	/** hook to modify damage CAUSED by this Pawn - note that EventInstigator may not be equal to Controller if we're in a vehicle, etc */
	UFUNCTION(BlueprintNativeEvent)
	void ModifyDamageCaused(int32& Damage, FVector& Momentum, const FDamageEvent& DamageEvent, AActor* Victim, AController* EventInstigator, AActor* DamageCauser);

	/** switches weapon locally, must execute independently on both server and client */
	virtual void LocalSwitchWeapon(AUTWeapon* NewWeapon);
	/** RPC to do weapon switch */
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSwitchWeapon(AUTWeapon* NewWeapon);
	UFUNCTION(Client, Reliable)
	virtual void ClientSwitchWeapon(AUTWeapon* NewWeapon);
	/** utility to redirect to SwitchToBestWeapon() to the character's Controller (human or AI) */
	void SwitchToBestWeapon();

	/** spawn/destroy/replace the current weapon attachment to represent the equipped weapon (through WeaponClass) */
	UFUNCTION()
	virtual void UpdateWeaponAttachment();

	// firemodes with input currently being held down (pending or actually firing)
	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	TArray<uint8> PendingFire;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Pawn")
	AUTInventory* InventoryList;

	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	AUTWeapon* PendingWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	class AUTWeapon* Weapon;

	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	class AUTWeaponAttachment* WeaponAttachment;

	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing=UpdateWeaponAttachment, Category = "Pawn")
	TSubclassOf<AUTWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, Category = "Pawn")
	TArray< TSubclassOf<AUTInventory> > DefaultCharacterInventory;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=AmbientSoundUpdated, Category = "Pawn")
	USoundBase* AmbientSound;
	UPROPERTY(BlueprintReadOnly, Category = "Pawn")
	UAudioComponent* AmbientSoundComp;

	/** last time PlayFootstep() was called, for timing footsteps when animations are disabled */
	float LastFootstepTime;
	/** last FootNum for PlayFootstep(), for alternating when animations are disabled */
	uint8 LastFoot;
	
	virtual void BecomeViewTarget(class APlayerController* PC) override;
	virtual void EndViewTarget( class APlayerController* PC );

	/** replicated overlays, bits match entries in UTGameState's OverlayMaterials array */
	UPROPERTY(Replicated, ReplicatedUsing = UpdateCharOverlays)
	uint16 CharOverlayFlags;
	UPROPERTY(Replicated, ReplicatedUsing = UpdateWeaponOverlays)
	uint16 WeaponOverlayFlags;
	/** mesh with current active overlay material on it (created dynamically when needed) */
	UPROPERTY()
	USkeletalMeshComponent* OverlayMesh;

	/** replicated character material override */
	UPROPERTY(Replicated, ReplicatedUsing = UpdateSkin)
	UMaterialInterface* ReplicatedBodyMaterial;

	/** runtime material instance for setting body material parameters (team color, etc) */
	UPROPERTY(BlueprintReadOnly, Category = Pawn)
	UMaterialInstanceDynamic* BodyMI;
public:
	/** legacy command for dropping the flag.  Just redirects to UseCarriedObject */
	UFUNCTION(Exec)
	virtual void DropFlag();
protected:
	/** uses the current carried object */
	UFUNCTION(exec)
	virtual void DropCarriedObject();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerDropCarriedObject();

	/** uses the current carried object */
	UFUNCTION(exec)
	virtual void UseCarriedObject();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUseCarriedObject();


private:
	void ApplyDamageMomentum(float DamageTaken, FDamageEvent const& DamageEvent, APawn* PawnInstigator, AActor* DamageCauser)
	{
		UE_LOG(UT, Warning, TEXT("Use TakeDamage() instead"));
		checkSlow(false);
	}
};

inline bool AUTCharacter::IsDead()
{
	return bTearOff || bPendingKillPending;
}


