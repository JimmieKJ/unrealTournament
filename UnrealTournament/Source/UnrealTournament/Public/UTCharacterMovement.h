// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacterMovement.generated.h"

UCLASS()
class UUTCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY()

public:

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual bool ClientUpdatePositionAfterServerUpdate() override;

	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;

	virtual FVector GetImpartedMovementBaseVelocity() const override;

	virtual bool CanCrouchInCurrentState() const override;

	virtual void PerformMovement(float DeltaSeconds) override;

	virtual FVector ConsumeInputVector() override;

	/** Try to base on lift that just ran into me, return true if success */
	virtual bool CanBaseOnLift(UPrimitiveComponent* LiftPrim, const FVector& LiftMoveDelta);

	/** @TODO FIXMESTEVE remove when we get UnableToFollowBaseMove() notify in base engine */
	virtual void UpdateBasedMovement(float DeltaSeconds) override;

	/** If I'm on a lift, tell it to return */
	virtual void UnableToFollowBaseMove(FVector DeltaPosition, FVector OldLocation);

	/** @TODO FIXMESTEVE remove when we get UE4 4.4
	* Determine whether we should try to find a valid landing spot after an impact with an invalid one (based on the Hit result).
	* For example, landing on the lower portion of the capsule on the edge of geometry may be a walkable surface, but could have reported an unwalkable impact normal.
	*/
	virtual bool ShouldCheckForValidLandingSpot(const float DeltaTime, const FVector& Delta, const FHitResult& Hit) const;

	/** Reset timers (called on pawn possessed) */
	virtual void ResetTimers();

	/** Smoothed speed */
	UPROPERTY()
	float AvgSpeed;

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

	/** If falling, verify can wall dodge.  The cause character to dodge. */
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

	/** Acceleration at start of last PerformMovement. */
	UPROPERTY(Category = "Saved Acceleration", BlueprintReadOnly)
	FVector SavedAcceleration;

	/** If true, keep old acceleration while holding bWantSlideRoll if no movement keys are pressed. */
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	bool bMaintainSlideRollAccel;
	
protected:
	/** True if player is holding modifier to slide/roll.  Change with UpdateSlideRoll(). */
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	bool bWantsSlideRoll;

public:
	/** If true, auto-slide, otherwise need to hold shift down to slide along walls. */
	UPROPERTY(EditAnywhere, Category = Movement)
	bool bAutoSlide;

	/** Horizontal speed reduction on roll ending (multiplied). */
	UPROPERTY(Category = "DodgeRoll", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Dodge Landing Speed Factor"))
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

	/** Dodge roll out (holding bRollSlide while dodging on ground) */
	virtual void PerformRoll(const FVector& DodgeDir);

	// Flags used to synchronize dodging in networking (analoguous to bPressedJump)
	bool bPressedDodgeForward;
	bool bPressedDodgeBack;
	bool bPressedDodgeLeft;
	bool bPressedDodgeRight;

	/** Return true if character can dodge. */
	virtual bool CanDodge();

	/** Return true if character can jump. */
	virtual bool CanJump();

	/** Clear dodging input related flags */
	void ClearJumpInput();

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

	/** Whether to allow multijumps during a dodge. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Allow Dodge Multijumps"))
	bool bAllowDodgeMultijumps;

	/** Whether to allow multijumps during a jump. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Allow Dodge Multijumps"))
		bool bAllowJumpMultijumps;

	/** Max absolute Z velocity allowed for multijump (low values mean only near jump apex). */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max Multijump Z Speed"))
	float MaxMultiJumpZSpeed;

	/** Vertical impulse on multijump. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Multijump impulse (vertical)"))
	float MultiJumpImpulse;

	/** Vertical impulse on dodge jump. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Multijump impulse (vertical)"))
		float DodgeJumpImpulse;

	/** Air control during multijump . */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Multijump)
	float MultiJumpAirControl;

	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

	virtual bool DoJump() override;

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

	UPROPERTY(Category = "Wall Slide", BlueprintReadOnly)
	bool bApplyWallSlide;

	virtual void HandleImpact(FHitResult const& Impact, float TimeSlice, const FVector& MoveDelta) override;

	virtual float GetGravityZ() const override;
	
	/** Set bApplyWallSlide if should slide against wall we are currently touching */
	virtual void CheckWallSlide(FHitResult const& Impact);

	//=========================================
	// Networking

	/** Time server is using for this move, from timestamp passed by client */
	UPROPERTY()
	float CurrentServerMoveTime;

	/** Return world time on client, CurrentClientTimeStamp on server */
	virtual float GetCurrentMovementTime() const;

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
	}

	// Flags used to synchronize dodging in networking (analoguous to bPressedJump)
	bool bPressedDodgeForward;
	bool bPressedDodgeBack;
	bool bPressedDodgeLeft;
	bool bPressedDodgeRight;
	bool bSavedIsSprinting;
	bool bSavedIsRolling;
	bool bSavedWantsSlide;

	// Flag to plant character during emoting
	bool bSavedIsEmoting;

	virtual void Clear() override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
};


class FNetworkPredictionData_Client_UTChar : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	/** Allocate a new saved move. Subclasses should override this if they want to use a custom move class. */
	virtual FSavedMovePtr AllocateNewMove() override;
};





