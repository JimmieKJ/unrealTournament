// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCharacterMovement.generated.h"

UCLASS()
class UUTCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY()

public:
	//=========================================
	// DODGING
	/** Dodge impulse in XY plane */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Horizontal Dodge Impulse"))
	float DodgeImpulseHorizontal;

	/** Dodge impulse added in Z direction */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Vertical Dodge Impulse"))
	float DodgeImpulseVertical;

	/** How far to trace looking for a wall to dodge from */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Wall Dodge Trace Distance"))
	float WallDodgeTraceDist;

	/** Vertical impulse for a wall dodge. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Wall Dodge Vertical Impulse is Additive"))
	float WallDodgeImpulseVertical;

	/** Max Falling speed without additive Wall Dodge Vertical Impulse.  If falling faster, vertical dodge impulse is added to current falling velocity event if bAdditiveVerticalImpulse is false. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max Falling speed without additive Wall Dodge Vertical Impulse"))
	float MaxNonAdditiveDodgeFallSpeed;

	/** Max positive Z speed with additive Wall Dodge Vertical Impulse.  Wall Dodge will not add impulse making vertical speed higher than this amount. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max jumping speed with additive Wall Dodge Vertical Impulse"))
	float MaxAdditiveDodgeJumpSpeed;

	/** Horizontal speed reduction on dodge landing (multiplied). */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Horizontal speed reduction on dodge landing (multiplied)"))
		float DodgeLandingSpeedFactor;

	/** Time after landing dodge before another can be attempted. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Time after landing dodge before another can be attempted."))
		float DodgeResetInterval;

	/** Time after starting wall dodge before another can be attempted. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Time after starting wall dodge before another can be attempted."))
		float WallDodgeResetInterval;

	/** World time when another dodge can be attempted. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "World time when another dodge can be attempted."))
		float DodgeResetTime;

	/** If falling, verify can wall dodge.  The cause character to dodge. */
	UFUNCTION()
	bool PerformDodge(const FVector &DodgeDir, const FVector &DodgeCross);

	/** True during a dodge. */
	UPROPERTY(Category = "Dodging", EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Currently in a dodge."))
	bool bIsDodging;

	// Flags used to synchronize dodging in networking (analoguous to bPressedJump)
	bool bPressedDodgeForward;
	bool bPressedDodgeBack;
	bool bPressedDodgeLeft;
	bool bPressedDodgeRight;

	/** Return true if character can dodge. */
	virtual bool CanDodge();

	/** Clear dodging input related flags */
	void ClearJumpInput();

	//=========================================
	// MULTIJUMP

	/** Max number of multijumps. 2= double jump. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max number of multijumps"))
	float MaxMultiJumpCount;

	/** Current count of multijumps. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Current count of multijumps"))
	float CurrentMultiJumpCount;

	/** Whether to allow multijumps during a dodge. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Allow multijump from dodge"))
	bool bAllowDodgeMultijumps;

	/** Max absolute Z velocity allowed for multijump (low values mean only near jump apex). */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max falling speed to multijump"))
	float MaxMultiJumpZSpeed;

	/** Vertical impulse on multijump. */
	UPROPERTY(Category = "Multijump", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Multijump vertical impulse"))
	float MultiJumpImpulse;

	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) OVERRIDE;

	virtual bool DoJump() OVERRIDE;

	/** Perform a multijump */
	virtual bool DoMultiJump();

	/** Return true if can multijump now. */
	virtual bool CanMultiJump();

	//=========================================
	// AUTO-SPRINT

	/** How long you have to be running/grounded before auto-sprint engages. */
	UPROPERTY(Category = "Autosprint", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "How long you have to be running/grounded before auto-sprint engages."))
		float AutoSprintDelayInterval;

	/** Max speed when sprinting. */
	UPROPERTY(Category = "Autosprint", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Max speed when sprinting."))
		float SprintSpeed;

	/** Acceleration when sprinting. */
	UPROPERTY(Category = "Autosprint", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Acceleration when sprinting."))
		float SprintAccel;

	/** World time when another dodge can be attempted. */
	UPROPERTY(Category = "Autosprint", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "World time when sprinting can start."))
		float SprintStartTime;

	/** True when sprinting. */
	UPROPERTY(Category = "Autosprint", EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Currently sprinting."))
		bool bIsSprinting;

	/** Reset sprint start if braking. */
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) OVERRIDE;

	/** Support for sprint acceleration. */
	virtual float GetModifiedMaxAcceleration() const OVERRIDE;

	/** Return true if character can sprint right now */
	virtual bool CanSprint() const;

	/** Return SprintSpeed if CanSprint(). */
	virtual float GetMaxSpeed() const OVERRIDE;

	//=========================================

	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const OVERRIDE;
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

	virtual void Clear() OVERRIDE;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) OVERRIDE;
	virtual uint8 GetCompressedFlags() const OVERRIDE;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const OVERRIDE;
};


class FNetworkPredictionData_Client_UTChar : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	/** Allocate a new saved move. Subclasses should override this if they want to use a custom move class. */
	virtual FSavedMovePtr AllocateNewMove() OVERRIDE;
};





