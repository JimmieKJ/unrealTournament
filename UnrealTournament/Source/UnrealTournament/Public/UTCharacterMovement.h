// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacterMovement.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void MoveSmooth(const FVector& InVelocity, const float DeltaSeconds, FStepDownResult* OutStepDownResult = NULL) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;
	virtual void ReplicateMoveToServer(float DeltaTime, const FVector& NewAcceleration) override;
	virtual void ClientAckGoodMove_Implementation(float TimeStamp) override;

	/** Return true if it is OK to delay sending this player movement to the server to conserve bandwidth. */
	virtual bool CanDelaySendingMove(const FSavedMovePtr& NewMove) override;

	virtual void UTServerMoveHandleClientError(float TimeStamp, float DeltaTime, const FVector& Accel, const FVector& RelativeClientLoc, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);

	virtual void UTCallServerMove();

	UPROPERTY(Category = "Character Movement", BlueprintReadOnly)
		float MaxPositionErrorSquared;

	/** @return true if position error exceeds max allowable amount */
	virtual bool ExceedsAllowablePositionError(FVector LocDiff) const;

	/** Process servermove forwarded by character */
	virtual void ProcessServerMove(float TimeStamp, FVector Accel, FVector ClientLoc, uint8 CompressedMoveFlags, float ViewYaw, float ViewPitch, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);

	/** Process servermove forwarded by character, without correction */
	virtual void ProcessQuickServerMove(float TimeStamp, FVector Accel, uint8 CompressedMoveFlags);

	/** Process servermove forwarded by character, without correction, but needs rotation */
	virtual void ProcessSavedServerMove(float TimeStamp, FVector Accel, uint8 CompressedMoveFlags, float ViewYaw, float ViewPitch);

	/** Process old servermove forwarded by character */
	virtual void ProcessOldServerMove(float OldTimeStamp, FVector OldAccel, float OldYaw, uint8 OldMoveFlags);

	/** Sets LastClientAdjustmentTime so there will be no delay in sending any needed adjustment. */
	virtual void NeedsClientAdjustment();

	/** Last time a client adjustment was sent.  Used to limit frequency (for when client hasn't had a chance to respond yet. */
	UPROPERTY()
	float LastClientAdjustmentTime;

	/** Last time a good move ack was sent.  Used to limit frequency (for when client hasn't had a chance to respond yet. */
	UPROPERTY()
		float LastGoodMoveAckTime;

	/** Set if the pending client adjustment is for a large position correction. */
	UPROPERTY()
		bool bLargeCorrection;

	/** Position error considered a large correction for client adjustment. */
	UPROPERTY()
		float LargeCorrectionThreshold;

	/** Used to control minimum frequency of client adjustment and good move ack replication. */
	UPROPERTY()
	float MinTimeBetweenClientAdjustments;

	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	virtual FVector GetImpartedMovementBaseVelocity() const override;
	virtual bool CanCrouchInCurrentState() const override;
	virtual void PerformMovement(float DeltaSeconds) override;
	virtual float ComputeAnalogInputModifier() const override;

	/** Try to base on lift that just ran into me, return true if success */
	virtual bool CanBaseOnLift(UPrimitiveComponent* LiftPrim, const FVector& LiftMoveDelta);

	virtual void UpdateBasedMovement(float DeltaSeconds) override;

	/** If I'm on a lift, tell it to return */
	virtual void OnUnableToFollowBaseMove(const FVector& DeltaPosition, const FVector& OldLocation, const FHitResult& MoveOnBaseHit) override;
	
	/** Reset timers (called on pawn possessed) */
	virtual void ResetTimers();

	/** Return true if movement input should not be constrained to horizontal plane */
	virtual bool Is3DMovementMode() const;

	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	/** Allows custom handling of timestamp and delta time updates, including resetting movement timers. */
	virtual float UpdateTimeStampAndDeltaTime(float DeltaTime, FNetworkPredictionData_Client_Character* ClientData);

	/** Adjust movement timers after timestamp reset */
	virtual void AdjustMovementTimers(float Adjustment);

	/** Allows custom handling of timestamp and delta time updates, including resetting movement timers. */
	virtual bool UTVerifyClientTimeStamp(float TimeStamp, FNetworkPredictionData_Server_Character & ServerData);

	/** for replaying moves set up */
	bool bIsSettingUpFirstReplayMove;

	/** Smoothed speed */
	UPROPERTY()
	float AvgSpeed;

	/** Max Acceleration when falling (will be scaled by AirControl property). */
	UPROPERTY(Category = "Character Movement", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float MaxFallingAcceleration;

	/** Max speed player can travel in water (faster than powered swim speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Swimming)
		float MaxWaterSpeed;

	/** Impulse when pushing off wall underwater */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Swimming)
	float SwimmingWallPushImpulse;

	virtual void PhysSwimming(float deltaTime, int32 Iterations) override;

	/** True if was swimming in current, and haven't landed yet. */
	UPROPERTY(BlueprintReadOnly, Category = Swimming)
		bool bFallingInWater;

	/** Apply water current to swimming or falling player in contact with water. */
	virtual void ApplyWaterCurrent(float DeltaTime);

	virtual bool ShouldJumpOutOfWater(FVector& JumpDir) override;

	/** Push off bottom while swimming. */
	virtual void PerformWaterJump();

	/** Last Z position when standing on ground - used for eyeheight smoothing */
	UPROPERTY()
	float OldZ;

	/** Impulse imparted by "easy" impact jump. Not charge or jump dependent (although get a small bonus with timed jump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
	float EasyImpactImpulse;

	/** Impulse imparted by "easy" impact jump. Not charge or jump dependent (although get a small bonus with timed jump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
	float EasyImpactDamage;

	/** Impulse imparted by "easy" impact jump. Not charge or jump dependent (although get a small bonus with timed jump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
		float FullImpactImpulse;

	/** Impulse imparted by "easy" impact jump. Not charge or jump dependent (although get a small bonus with timed jump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
		float FullImpactDamage;

	/** Max total horizontal velocity after impact jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
		float ImpactMaxHorizontalVelocity;

	/** Scales impact impulse to determine max total vertical velocity after impact jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
		float ImpactMaxVerticalFactor;

	/** Add an impulse, damped if player would end up going too fast */
	UFUNCTION(BlueprintCallable, Category = "Impulse")
		virtual void ApplyImpactVelocity(FVector JumpDir, bool bIsFullImpactImpulse);

	/** Max undamped Z Impulse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ImpactJumping)
	float MaxUndampedImpulse;

	/** Add an impulse, damped if player would end up going too fast */
	UFUNCTION(BlueprintCallable, Category = "Impulse")
	virtual void AddDampedImpulse(FVector Impulse, bool bSelfInflicted);

	/** Clear pending impulse. */
	virtual void ClearPendingImpulse();

	/** true if projectile/hitscan spawned this frame (replicated to synchronize held firing) */
UPROPERTY()
	bool bShotSpawned;

	//=========================================
	// DODGING
	/** Dodge impulse in XY plane */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Dodge Impulse- Horizontal"))
	float DodgeImpulseHorizontal;

	/** Dodge impulse added in Z direction */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Dodge Impulse - Vertical"))
	float DodgeImpulseVertical;

	/** How far to trace looking for a wall to dodge from */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Wall Dodge Trace Distance"))
	float WallDodgeTraceDist;

	/** Wall Dodge impulse in XY plane */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Wall Dodge Impulse- Horizontal"))
		float WallDodgeImpulseHorizontal;

	/** Vertical impulse for first wall dodge. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Wall Dodge Impulse Vertical"))
	float WallDodgeImpulseVertical;

	/** Vertical impulse for subsequent consecutive wall dodges. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Wall Dodge Second Impulse Vertical"))
		float WallDodgeSecondImpulseVertical;

	/** Grace negative velocity which is zeroed before adding walldodgeimpulse */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite)
		float WallDodgeGraceVelocityZ;

	/** Minimum Normal of Wall Dodge from wall (1.0 is 90 degrees, 0.0 is along wall, 0.7 is 45 degrees). */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Wall Dodge Min Normal"))
		float WallDodgeMinNormal;

	/** Wall normal of most recent wall dodge */
	UPROPERTY(Category = "Dodging", BlueprintReadOnly)
	FVector LastWallDodgeNormal;

	/** Max dot product of previous wall normal and new wall normal to perform a dodge (to prevent dodging along a wall) */
	UPROPERTY(Category = "Dodging", BlueprintReadOnly)
	float MaxConsecutiveWallDodgeDP;

	/** Max number of consecutive wall dodges without landing. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max Wall Dodges"))
		int32 MaxWallDodges;

	/** Current count of wall dodges. */
	UPROPERTY(Category = "Dodging", BlueprintReadOnly, meta = (DisplayName = "Current Wall Dodge Count"))
		int32 CurrentWallDodgeCount;

	/** Time after starting wall dodge before another can be attempted. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Wall Dodge Reset Interval"))
		float WallDodgeResetInterval;

	/** If falling faster than this speed, then don't add wall dodge impulse. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Min Additive Dodge Fall Speed"))
	float MinAdditiveDodgeFallSpeed;

	/** Max positive Z speed with additive Wall Dodge Vertical Impulse.  Wall Dodge will not add impulse making vertical speed higher than this amount. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max Additive Dodge Jump Speed"))
	float MaxAdditiveDodgeJumpSpeed;

	/** Horizontal speed reduction on dodge landing (multiplied). */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Dodge Landing Speed Factor"))
		float DodgeLandingSpeedFactor;

	/** Horizontal speed reduction on dodge jump landing (multiplied). */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Dodge Landing Speed Factor"))
		float DodgeJumpLandingSpeedFactor;

	/** Time after landing dodge before another can be attempted. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Dodge Reset Interval"))
		float DodgeResetInterval;

	/** Time after landing dodge-jump before another can be attempted. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Dodge Jump Reset Interval"))
		float DodgeJumpResetInterval;

	/** Maximum XY velocity of dodge (dodge impulse + current movement combined). */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Dodge Max Horizontal Velocity"))
		float DodgeMaxHorizontalVelocity;

	/** World time when another dodge can be attempted. */
	UPROPERTY(Category = "Dodging", BlueprintReadWrite)
		float DodgeResetTime;

	/** If falling, verify can wall dodge.  This causes character to dodge. */
	UFUNCTION()
	bool PerformDodge(FVector &DodgeDir, FVector &DodgeCross);

	/** True during a dodge. */
	UPROPERTY(Category = "Dodging", BlueprintReadOnly)
	bool bIsDodging;

	/** True during a dodge roll. */
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	bool bIsDodgeRolling;

	/** True if was dodge rolling last movement update. */
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	bool bWasDodgeRolling;

	UPROPERTY(Category = "Emote", BlueprintReadOnly)
	bool bIsEmoting;
	
protected:
	/** True if player is holding modifier to slide/roll.  Change with UpdateSlideRoll(). */
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	bool bWantsSlideRoll;

public:
	/** If true, auto-slide, otherwise need to hold shift down to slide along walls. */
	UPROPERTY(EditAnywhere, Category = Movement)
	bool bAutoSlide;

	/** Half height of capsule when rolling */
	UPROPERTY(Category = "DodgeRoll", EditAnywhere, BlueprintReadWrite)
	float RollHalfHeight;

	/** Horizontal speed reduction on roll ending (multiplied). */
	UPROPERTY(Category = "DodgeRoll", EditAnywhere, BlueprintReadWrite)
	float RollEndingSpeedFactor;

	/** Acceleration during a dodge roll. */
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	float DodgeRollAcceleration;

	/** Max speed during a dodge roll. */
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	float MaxDodgeRollSpeed;

	/** How long dodge roll lasts. */
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	float DodgeRollDuration;

	/** When dodge roll ends. */
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	float DodgeRollEndTime;

	/** When dodge roll button was last tapped. */
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	float DodgeRollTapTime;

	/** Maximum interval dodge roll tap can be performed before landing dodge to get bonus. */
	UPROPERTY(Category = "DodgeRoll", EditAnywhere, BlueprintReadWrite)
	float DodgeRollBonusTapInterval;

	/** Falling damage reduction if hit roll within DodgeRollBonusTapInterval */
	UPROPERTY(Category = "DodgeRoll", EditAnywhere, BlueprintReadWrite)
	float FallingDamageRollReduction;

	/** Amount of falling damage reduction */
	UFUNCTION(BlueprintCallable, Category = "DodgeRoll")
	virtual	float FallingDamageReduction(float FallingDamage, const FHitResult& Hit);

	/** Maximum Velocity Z that a Dodge Roll tap will register. */
	UPROPERTY(Category = "DodgeRoll", EditAnywhere, BlueprintReadWrite)
	float DodgeRollEarliestZ;

	/** Enables slope dodge boost. */
	UPROPERTY(Category = "DodgeRoll", EditAnywhere, BlueprintReadOnly)
	bool bAllowSlopeDodgeBoost;

	/** Affects amount of slope dodge possible. */
	UPROPERTY(Category = "DodgeRoll", EditAnywhere, BlueprintReadOnly)
	float SlopeDodgeScaling;

	/** Update bWantsSlideRoll and DodgeRollTapTime */
	UFUNCTION(BlueprintCallable, Category = "DodgeRoll")
	virtual void UpdateSlideRoll(bool bNewWantsSlideRoll);

	/** Update bWantsSlideRoll and DodgeRollTapTime */
	UFUNCTION(BlueprintCallable, Category = "DodgeRoll")
	virtual bool WantsSlideRoll();

	virtual void Crouch(bool bClientSimulation = false) override;

	/** Dodge roll out (holding bRollSlide while dodging on ground) */
	virtual void PerformRoll(const FVector& DodgeDir);

	virtual bool IsCrouching() const override;

	// Flags used to synchronize dodging in networking (analoguous to bPressedJump)
	bool bPressedDodgeForward;
	bool bPressedDodgeBack;
	bool bPressedDodgeLeft;
	bool bPressedDodgeRight;

	/** Flag to synchronize tap slide */
	bool bPressedSlide;

	/** Return true if character can dodge. */
	virtual bool CanDodge();

	/** Return true if character can jump. */
	virtual bool CanJump();

	/** Clear dodging input related flags */
	virtual void ClearDodgeInput();

	/** Handle jump inputs */
	virtual void CheckJumpInput(float DeltaTime);

	/** Optionally allow slope dodge */
	virtual FVector ComputeSlideVectorUT(const float DeltaTime, const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit);

	//=========================================
	// MULTIJUMP

	/** Max number of multijumps. 2= double jump. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max Multijump Count"))
	int32 MaxMultiJumpCount;

	/** Current count of multijumps. */
	UPROPERTY(Category = "Multijump", BlueprintReadWrite, meta = (DisplayName = "Current Multijump Count"))
	int32 CurrentMultiJumpCount;

	/** True if player is falling because of a jump or dodge. */
	UPROPERTY(Category = "Multijump", BlueprintReadWrite)
	bool bExplicitJump;

	/** Whether to allow multijumps during a dodge. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite)
	bool bAllowDodgeMultijumps;

	/** Whether to allow multijumps during a jump. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite)
	bool bAllowJumpMultijumps;

	/** Max absolute Z velocity allowed for multijump (low values mean only near jump apex). */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max Multijump Z Speed"))
	float MaxMultiJumpZSpeed;
	/** if set, always allow multijump while Velocity.Z < 0 */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite)
	bool bAlwaysAllowFallingMultiJump;

	/** Vertical impulse on multijump. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Multijump impulse (vertical)"))
	float MultiJumpImpulse;

	/** Vertical impulse on dodge jump. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Dodge jump impulse (vertical)"))
	float DodgeJumpImpulse;

	/** Air control during multijump . */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Multijump)
	float MultiJumpAirControl;

	/** if set air control is forced to zero during the current jump/fall */
	UPROPERTY(BlueprintReadWrite, Category = AirControl)
	bool bRestrictedJump;

	FTimerHandle ClearRestrictedJumpHandle;

	/** Call to restrict air control for part of a jump/fall. */
	virtual void RestrictJump(float RestrictedJumpTime);

	/** allows timed clearing of bRestrictedJump during the jump/fall (e.g. jump pads that only want to restrict for part of the jump) */
	UFUNCTION()
	virtual void ClearRestrictedJump();

	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

	virtual bool DoJump(bool bReplayingMoves) override;

	/** Perform a multijump */
	virtual bool DoMultiJump();

	/** Return true if can multijump now. */
	virtual bool CanMultiJump();

	//=========================================
	// AUTO-SPRINT

	/** How long you have to be running/grounded before auto-sprint engages. */
	UPROPERTY(Category = "Autosprint", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Auto Sprint Delay Interval"))
		float AutoSprintDelayInterval;

	/** Max speed when sprinting. */
	UPROPERTY(Category = "Autosprint", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Sprint Speed"))
		float SprintSpeed;

	/** Acceleration when sprinting. */
	UPROPERTY(Category = "Autosprint", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Sprint Acceleration"))
		float SprintAccel;

	/** World time when sprinting can start. */
	UPROPERTY(Category = "Autosprint", BlueprintReadWrite, meta = (DisplayName = "Sprint Start Time"))
		float SprintStartTime;

	/** True when sprinting. */
	UPROPERTY(Category = "Autosprint", EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Is sprinting"))
		bool bIsSprinting;

	/** Reset sprint start if braking. */
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;

	/** Support for sprint acceleration. */
	virtual float GetMaxAcceleration() const override;

	/** Return true if character can sprint right now */
	virtual bool CanSprint() const;

	/** Return SprintSpeed if CanSprint(). */
	virtual float GetMaxSpeed() const override;

	//=========================================
	// Landing Assist

	/** Defines what jumps just barely missed */
	UPROPERTY(Category = "LandingAssist", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Landing Step Up Distance"))
	float LandingStepUp;

	/** Boost to help successfully land jumps that just barely missed */
	UPROPERTY(Category = "LandingAssist", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Landing Assist Boost"))
	float LandingAssistBoost;

	/** True if already assisted this jump */
	UPROPERTY(Category = "LandingAssist", BlueprintReadOnly, meta = (DisplayName = "Jump Assisted"))
		bool bJumpAssisted;

	virtual void PhysFalling(float deltaTime, int32 Iterations) override;

	/** Return true if found a landing assist spot, and add LandingAssistBoost */
	virtual void FindValidLandingSpot(const FVector& CapsuleLocation);

	/** Check for landing assist */
	virtual void NotifyJumpApex() override;

	//=========================================
	// Wall Slide

	/**  Max Falling velocity Z to get slide */
	UPROPERTY(Category = "Wall Slide", EditAnywhere, BlueprintReadWrite)
	float MaxSlideFallZ;

	/** Gravity acceleration reduction during wall slide */
	UPROPERTY(Category = "Wall Slide", EditAnywhere, BlueprintReadWrite)
	float SlideGravityScaling;

	/** Minimum horizontal velocity to wall slide */
	UPROPERTY(Category = "Wall Slide", EditAnywhere, BlueprintReadWrite)
	float MinWallSlideSpeed;

	/** Maximum dot product of acceleration and wall normal (more negative means accel pushing more into wall) */
	UPROPERTY(Category = "Wall Slide", EditAnywhere, BlueprintReadWrite)
	float MaxSlideAccelNormal;

	/** If true, the player is against the wall and WallSlideNormal will describe the touch. */
	UPROPERTY(Category = "Wall Slide", BlueprintReadOnly)
	bool bIsAgainstWall;

	/** Used to gate client=side checking whether other characters are falling against a wall. */
	UPROPERTY()
	float LastCheckedAgainstWall;

	/** Normal of the wall we are sliding against. */
	UPROPERTY(Category = "Wall Slide", BlueprintReadOnly)
	FVector WallSlideNormal;

	virtual void HandleImpact(FHitResult const& Impact, float TimeSlice, const FVector& MoveDelta) override;

	virtual float GetGravityZ() const override;
	
	/** Set bApplyWallSlide if should slide against wall we are currently touching */
	virtual void CheckWallSlide(FHitResult const& Impact);

	//=========================================
	// Networking

	virtual void SendClientAdjustment() override;

	virtual void SmoothClientPosition(float DeltaTime) override;

	virtual void SimulateMovement(float DeltaTime) override;
	virtual void SimulateMovement_Internal(float DeltaTime);

	/** Used for remote client simulation */
	UPROPERTY()
	FVector SimulatedVelocity;

	/** Time server is using for this move, from timestamp passed by client */
	UPROPERTY()
	float CurrentServerMoveTime;

	/** Return world time on client, CurrentClientTimeStamp on server */
	virtual float GetCurrentMovementTime() const;

	/** Return synchronized time (timestamp currently being used on server, timestamp being sent on client) */
	virtual float GetCurrentSynchTime() const;

	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	virtual void ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode) override;
};

// Networking support
class FSavedMove_UTCharacter : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	FSavedMove_UTCharacter()
	{
		AccelMagThreshold = 2000.f;
		AccelDotThreshold = 0.8f;
		bShotSpawned = false;
	}

	/** true if projectile/hitscan spawned this frame, not from firing press/release. */
	bool bShotSpawned;

	// Flags used to synchronize dodging in networking (analoguous to bPressedJump)
	bool bPressedDodgeForward;
	bool bPressedDodgeBack;
	bool bPressedDodgeLeft;
	bool bPressedDodgeRight;
	bool bSavedIsSprinting;
	bool bSavedIsRolling;
	bool bSavedWantsSlide;
	bool bPressedSlide;

	// Flag to plant character during emoting
	bool bSavedIsEmoting;

	// local only properties (not replicated) used when replaying moves
	int32 SavedMultiJumpCount;
	int32 SavedWallDodgeCount;
	float SavedSprintStartTime;
	float SavedDodgeResetTime;
	float SavedDodgeRollEndTime;
	bool bSavedJumpAssisted;
	bool bSavedIsDodging;

	// return true if rotation affects this moves implementation
	virtual bool NeedsRotationSent() const;

	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	virtual bool IsImportantMove(const FSavedMovePtr& LastAckedMove) const override;
	virtual void PostUpdate(class ACharacter* C, EPostUpdateMode PostUpdateMode) override;
	virtual void PrepMoveFor(class ACharacter* C) override;
};


class FNetworkPredictionData_Client_UTChar : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	/** Allocate a new saved move. Subclasses should override this if they want to use a custom move class. */
	virtual FSavedMovePtr AllocateNewMove() override;
};





