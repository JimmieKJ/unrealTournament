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

	/** @TODO FIXMESTEVE remove when we get UE4 4.4
	* Determine whether we should try to find a valid landing spot after an impact with an invalid one (based on the Hit result).
	* For example, landing on the lower portion of the capsule on the edge of geometry may be a walkable surface, but could have reported an unwalkable impact normal.
	*/
	virtual bool ShouldCheckForValidLandingSpot(const float DeltaTime, const FVector& Delta, const FHitResult& Hit) const;

	/** Smoothed speed */
	UPROPERTY()
	float AvgSpeed;

	/** Last Z position when standing on ground - used for eyeheight smoothing */
	UPROPERTY()
	float OldZ;
	
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

	/** Time after landing dodge before another can be attempted. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Dodge Reset Interval"))
		float DodgeResetInterval;

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
	UPROPERTY(Category = "Dodging", BlueprintReadOnly)
	bool bIsDodgeRolling;

	/** True during a dodge roll. */
	UPROPERTY(Category = "Dodging", BlueprintReadOnly)
	float DodgeRollAcceleration;

	/** How long dodge roll lasts. */
	UPROPERTY(Category = "Dodging", BlueprintReadOnly)
	float DodgeRollDuration;

	/** When dodge roll ends. */
	UPROPERTY(Category = "Dodging", BlueprintReadOnly)
	float DodgeRollEndTime;

	/** When dodge roll button was last tapped. */
	UPROPERTY(Category = "Dodging", BlueprintReadOnly)
	float DodgeRollTapTime;

	/** Maximum interval dodge roll tap can be performed before landing dodge. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite)
	float DodgeRollTapInterval;

	/** Enables slope dodge boost. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadOnly)
	bool bAllowSlopeDodgeBoost;

	/** Affects amount of slope dodge possible. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadOnly)
	float SlopeDodgeScaling;

	// Flags used to synchronize dodging in networking (analoguous to bPressedJump)
	bool bPressedDodgeForward;
	bool bPressedDodgeBack;
	bool bPressedDodgeLeft;
	bool bPressedDodgeRight;

	/** Return true if character can dodge. */
	virtual bool CanDodge();

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

	/** Max absolute Z velocity allowed for multijump (low values mean only near jump apex). */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max Multijump Z Speed"))
	float MaxMultiJumpZSpeed;

	/** Vertical impulse on multijump. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Multijump impulse (vertical)"))
	float MultiJumpImpulse;

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

	// FIXME REMOVE
	void SetGravityScale(float NewGravityScale);

	//=========================================
	// Networking

	/** Time server is using for this move, from timestamp passed by client */
	UPROPERTY()
	float CurrentServerMoveTime;

	/** Return world time on client, CurrentClientTimeStamp on server */
	virtual float GetCurrentMovementTime() const;

	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
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





