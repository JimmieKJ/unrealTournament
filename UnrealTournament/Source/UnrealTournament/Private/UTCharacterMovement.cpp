// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacterMovement.h"


UUTCharacterMovement::UUTCharacterMovement(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	JumpZVelocity = 650.f;
	MaxWalkSpeed = 800.f;
	DodgeImpulseHorizontal = 1300.f;
	DodgeImpulseVertical = 420.f;
	WallDodgeImpulseVertical = 200.f;
	WallDodgeTraceDist = 50.f;
	MaxNonAdditiveDodgeFallSpeed = -200.f;
	MaxAdditiveDodgeJumpSpeed = 750.f;
	CurrentMultiJumpCount = 0;
	MaxMultiJumpCount = 1;
	bAllowDodgeMultijumps = false;
	MaxMultiJumpZSpeed = 100.f;
	MultiJumpImpulse = 500.f;
	DodgeLandingSpeedFactor = 0.1f;
	DodgeResetInterval = 0.35f;
	WallDodgeResetInterval = 0.1f;
	DodgeResetTime = 0.f;
	SprintSpeed = 1200.f;
	bIsDodging = false;
	bIsSprinting = false;
	SprintAccel = 100.f;
	AutoSprintDelayInterval = 2.f;
	SprintStartTime = 0.f;
	LandingStepUp = 50.f;
	LandingAssistBoost = 400.f;
	bJumpAssisted = false;
}

bool UUTCharacterMovement::CanDodge()
{
	return (IsMovingOnGround() || IsFalling()) && CanEverJump() && !bWantsToCrouch && (GetCurrentMovementTime() > DodgeResetTime);
}

bool UUTCharacterMovement::PerformDodge(const FVector &DodgeDir, const FVector &DodgeCross)
{
	if (!HasValidData())
	{
		return false;
	}

	if (IsFalling())
	{
		// if falling, check if can perform wall dodge
		FVector TraceEnd = -1.f*DodgeDir;
		float PawnCapsuleRadius, PawnCapsuleHalfHeight;
		CharacterOwner->CapsuleComponent->GetScaledCapsuleSize(PawnCapsuleRadius, PawnCapsuleHalfHeight);
		float TraceBoxSize = FMath::Min(0.25f*PawnCapsuleHalfHeight, 0.7f*PawnCapsuleRadius);
		FVector TraceStart = CharacterOwner->GetActorLocation();
		TraceStart.Z -= 0.5f*TraceBoxSize;
		TraceEnd = TraceStart - (WallDodgeTraceDist+PawnCapsuleRadius-0.5f*TraceBoxSize)*DodgeDir;

		static const FName DodgeTag = FName(TEXT("Dodge"));
		FCollisionQueryParams QueryParams(DodgeTag, false, CharacterOwner);
		FHitResult Result;
		const bool bBlockingHit = GetWorld()->SweepSingle(Result, TraceStart, TraceEnd, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(), FCollisionShape::MakeSphere(TraceBoxSize), QueryParams);
		if ( !bBlockingHit )
		{
			return false;
		}
		DodgeResetTime = GetCurrentMovementTime() + WallDodgeResetInterval;
	}

	// perform the dodge
	float VelocityZ = Velocity.Z;
	Velocity = DodgeImpulseHorizontal*DodgeDir + (Velocity | DodgeCross)*DodgeCross;
	if (!IsFalling())
	{
		Velocity.Z = DodgeImpulseVertical;
	}
	else if (VelocityZ < MaxAdditiveDodgeJumpSpeed)
	{
		// vertical impulse may or may not be additive
		Velocity.Z = (VelocityZ < MaxNonAdditiveDodgeFallSpeed)
			? FMath::Min(VelocityZ + WallDodgeImpulseVertical, MaxAdditiveDodgeJumpSpeed)
			: WallDodgeImpulseVertical;
	}
	bIsDodging = true;
	bNotifyApex = true;
	CurrentMultiJumpCount++;
	SetMovementMode(MOVE_Falling);
	return true;
}

float UUTCharacterMovement::GetModifiedMaxAcceleration() const
{
	return (CanSprint() && (Velocity.SizeSquared() > MaxWalkSpeed*MaxWalkSpeed)) ? SprintAccel : Super::GetModifiedMaxAcceleration();
}

bool UUTCharacterMovement::CanSprint() const
{
	return CharacterOwner && IsMovingOnGround() && !IsCrouching() && (GetCurrentMovementTime() > SprintStartTime);
}

float UUTCharacterMovement::GetMaxSpeed() const
{
	return CanSprint() ? SprintSpeed : Super::GetMaxSpeed();
}

void UUTCharacterMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

float UUTCharacterMovement::GetCurrentMovementTime() const
{
	if (CharacterOwner->Role != ROLE_Authority)
	{
		FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
		return ServerData ? ServerData->CurrentClientTimeStamp : CharacterOwner->GetWorld()->GetTimeSeconds();
	}
	return CharacterOwner->GetWorld()->GetTimeSeconds();
}

void UUTCharacterMovement::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (CharacterOwner)
	{
		if (CharacterOwner->NotifyLanded(Hit))
		{
			CharacterOwner->Landed(Hit);
		}
		if (bIsDodging)
		{
			Velocity *= DodgeLandingSpeedFactor;
			DodgeResetTime = GetCurrentMovementTime() + DodgeResetInterval;
			bIsDodging = false;
		}
		SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
	}
	bJumpAssisted = false;
	CurrentMultiJumpCount = 0;
	if (IsFalling())
	{
		SetPostLandedPhysics(Hit);
	}

	StartNewPhysics(remainingTime, Iterations);
}

bool UUTCharacterMovement::DoJump()
{
	if ( IsFalling() ? DoMultiJump() : Super::DoJump())
	{
		bNotifyApex = true;
		CurrentMultiJumpCount++;
		return true;
	}

	return false;
}

bool UUTCharacterMovement::DoMultiJump()
{
	if (CharacterOwner)
	{
		Velocity.Z = MultiJumpImpulse;
		return true;
	}

	return false;
}

bool UUTCharacterMovement::CanMultiJump()
{
	return ((CurrentMultiJumpCount < MaxMultiJumpCount) && (!bIsDodging || bAllowDodgeMultijumps) && (FMath::Abs(Velocity.Z) < MaxMultiJumpZSpeed));
}


void UUTCharacterMovement::ClearJumpInput()
{
	bPressedDodgeForward = false;
	bPressedDodgeBack = false;
	bPressedDodgeLeft = false;
	bPressedDodgeRight = false;
}

//======================================================
// Networking Support

FSavedMovePtr FNetworkPredictionData_Client_UTChar::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_UTCharacter());
}

bool FSavedMove_UTCharacter::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	bool bPressedDodge = bPressedDodgeForward || bPressedDodgeBack || bPressedDodgeLeft || bPressedDodgeRight;
	bool bNewPressedDodge = ((FSavedMove_UTCharacter*)&NewMove)->bPressedDodgeForward || ((FSavedMove_UTCharacter*)&NewMove)->bPressedDodgeBack || ((FSavedMove_UTCharacter*)&NewMove)->bPressedDodgeLeft || ((FSavedMove_UTCharacter*)&NewMove)->bPressedDodgeRight;
	if (bPressedDodge || bNewPressedDodge)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

uint8 FSavedMove_UTCharacter::GetCompressedFlags() const
{
	uint8 Result = 0;

	if (bPressedJump)
	{
		Result |= 1;
	}

	if (bPressedDodgeForward)
	{
		Result |= (1 << 2);
	}
	else if (bPressedDodgeBack)
	{
		Result |= (2 << 2);
	}
	else if (bPressedDodgeLeft)
	{
		Result |= (3 << 2);
	}
	else if (bPressedDodgeRight)
	{
		Result |= (4 << 2);
	}

	return Result;
}

void FSavedMove_UTCharacter::Clear()
{
	Super::Clear();
	bPressedDodgeForward = false;
	bPressedDodgeBack = false;
	bPressedDodgeLeft = false;
	bPressedDodgeRight = false;
}

void FSavedMove_UTCharacter::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	UUTCharacterMovement* UTCharMov = Cast<UUTCharacterMovement>(Character->CharacterMovement);
	if (UTCharMov)
	{
		bPressedDodgeForward = UTCharMov->bPressedDodgeForward;
		bPressedDodgeBack = UTCharMov->bPressedDodgeBack;
		bPressedDodgeLeft = UTCharMov->bPressedDodgeLeft;
		bPressedDodgeRight = UTCharMov->bPressedDodgeRight;
	}
}

FNetworkPredictionData_Client* UUTCharacterMovement::GetPredictionData_Client() const
{
	// Should only be called on client in network games
	check(PawnOwner != NULL);
	check(PawnOwner->Role < ROLE_Authority);
	check(GetNetMode() == NM_Client);

	if (!ClientPredictionData)
	{
		UUTCharacterMovement* MutableThis = const_cast<UUTCharacterMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_UTChar();
	}

	return ClientPredictionData;
}

/** @TODO FIXMESTEVE - physfalling copied from base version and edited.  At some point should probably add some hooks to base version and use those instead. */
void UUTCharacterMovement::PhysFalling(float deltaTime, int32 Iterations)
{
	// Bound final 2d portion of velocity
	const float Speed2d = Velocity.Size2D();
	const float BoundSpeed = FMath::Max(Speed2d, GetModifiedMaxSpeed() * AnalogInputModifier);

	//bound acceleration, falling object has minimal ability to impact acceleration
	FVector FallAcceleration = Acceleration;
	FallAcceleration.Z = 0.f;
	bool bSkipLandingAssist = true;
	FHitResult Hit(1.f);
	if (!HasRootMotion())
	{
		// test for slope to avoid using air control to climb walls @TODO FIXMESTEVE USE THIS FOR landing assist also? - if no aircontrol and trace hit nothing, skip landing assist
		float TickAirControl = AirControl;
		if (TickAirControl > 0.0f && FallAcceleration.SizeSquared() > 0.f)
		{
			const float TestWalkTime = FMath::Max(deltaTime, 0.05f);
			const FVector TestWalk = ((TickAirControl * GetModifiedMaxAcceleration() * FallAcceleration.SafeNormal() + FVector(0.f, 0.f, GetGravityZ())) * TestWalkTime + Velocity) * TestWalkTime;
			if (!TestWalk.IsZero())
			{
				static const FName FallingTraceParamsTag = FName(TEXT("PhysFalling"));
				FHitResult Result(1.f);
				FCollisionQueryParams CapsuleQuery(FallingTraceParamsTag, false, CharacterOwner);
				FCollisionResponseParams ResponseParam;
				InitCollisionParams(CapsuleQuery, ResponseParam);
				const FVector PawnLocation = CharacterOwner->GetActorLocation();
				const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
				const bool bHit = GetWorld()->SweepSingle(Result, PawnLocation, PawnLocation + TestWalk, FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);
				if (bHit)
				{
					bSkipLandingAssist = false;
					// Only matters if we can't walk there
					if (!IsValidLandingSpot(Result.Location, Result))
					{
						TickAirControl = 0.f;
					}
				}
			}
		}

		float MaxAccel = GetModifiedMaxAcceleration() * TickAirControl;

		// Boost maxAccel to increase player's control when falling
		if ((Speed2d < 10.f) && (TickAirControl > 0.f) && (TickAirControl <= 0.05f)) //allow initial burst
		{
			MaxAccel = MaxAccel + (10.f - Speed2d) / deltaTime;
		}

		FallAcceleration = FallAcceleration.ClampMaxSize(MaxAccel);
	}

	float remainingTime = deltaTime;
	float timeTick = 0.1f;

	while ((remainingTime > 0.f) && (Iterations < 8))
	{
		Iterations++;
		timeTick = (remainingTime > 0.05f)
			? FMath::Min(0.05f, remainingTime * 0.5f)
			: remainingTime;

		remainingTime -= timeTick;
		const FVector OldLocation = CharacterOwner->GetActorLocation();
		const FRotator PawnRotation = CharacterOwner->GetActorRotation();
		bJustTeleported = false;

		FVector OldVelocity = Velocity;

		// Apply input
		if (!HasRootMotion())
		{
			// Acceleration = FallAcceleration for CalcVelocity, but we restore it after using it.
			TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);
			const float SavedVelZ = Velocity.Z;
			Velocity.Z = 0.f;
			CalcVelocity(timeTick, FallingLateralFriction, false, BrakingDecelerationFalling);
			Velocity.Z = SavedVelZ;
		}

		// Apply gravity
		Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), timeTick);

		if (bNotifyApex && CharacterOwner->Controller && (Velocity.Z <= 0.f))
		{
			// Just passed jump apex since now going down
			bNotifyApex = false;
			NotifyJumpApex();
		}

		if (!HasRootMotion())
		{
			// make sure not exceeding acceptable speed
			Velocity = Velocity.ClampMaxSize2D(BoundSpeed);
		}

		FVector Adjusted = 0.5f*(OldVelocity + Velocity) * timeTick;
		SafeMoveUpdatedComponent(Adjusted, PawnRotation, true, Hit);

		if (!HasValidData())
		{
			return;
		}

		if (IsSwimming()) //just entered water
		{
			remainingTime = remainingTime + timeTick * (1.f - Hit.Time);
			StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
			return;
		}
		else if (Hit.Time < 1.f)
		{
			if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
			{
				remainingTime += timeTick * (1.f - Hit.Time);
				if (!bJustTeleported && (Hit.Time > 0.1f) && (Hit.Time * timeTick > 0.003f))
				{
					Velocity = (CharacterOwner->GetActorLocation() - OldLocation) / (timeTick * Hit.Time);
				}
				ProcessLanded(Hit, remainingTime, Iterations);
				return;
			}
			else
			{
				if (!bSkipLandingAssist)
				{
					FindValidLandingSpot(UpdatedComponent->GetComponentLocation());
				}
				HandleImpact(Hit, deltaTime, Adjusted);

				if (Acceleration.SizeSquared2D() > 0.f)
				{
					// If we've changed physics mode, abort.
					if (!HasValidData() || !IsFalling())
					{
						return;
					}
				}

				const FVector OldHitNormal = Hit.Normal;
				const FVector OldHitImpactNormal = Hit.ImpactNormal;
				FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, OldHitNormal, Hit);

				if ((Delta | Adjusted) > 0.f)
				{
					SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
					if (Hit.Time < 1.f) //hit second wall
					{
						if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
						{
							remainingTime = 0.f;
							ProcessLanded(Hit, remainingTime, Iterations);
							return;
						}

						HandleImpact(Hit, timeTick, Delta);

						// If we've changed physics mode, abort.
						if (!HasValidData() || !IsFalling())
						{
							return;
						}

						TwoWallAdjust(Delta, Hit, OldHitNormal);

						// bDitch=true means that pawn is straddling two slopes, neither of which he can stand on
						bool bDitch = ((OldHitImpactNormal.Z > 0.f) && (Hit.ImpactNormal.Z > 0.f) && (FMath::Abs(Delta.Z) <= KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));
						SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
						if (Hit.Time == 0)
						{
							// if we are stuck then try to side step
							FVector SideDelta = (OldHitNormal + Hit.ImpactNormal).SafeNormal2D();
							if (SideDelta.IsNearlyZero())
							{
								SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0).SafeNormal();
							}
							SafeMoveUpdatedComponent(SideDelta, PawnRotation, true, Hit);
						}

						if (bDitch || IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit) || Hit.Time == 0)
						{
							remainingTime = 0.f;
							ProcessLanded(Hit, remainingTime, Iterations);
							return;
						}
						else if (GetPerchRadiusThreshold() > 0.f && Hit.Time == 1.f && OldHitImpactNormal.Z >= GetWalkableFloorZ())
						{
							// We might be in a virtual 'ditch' within our perch radius. This is rare.
							const FVector PawnLocation = CharacterOwner->GetActorLocation();
							const float ZMovedDist = FMath::Abs(PawnLocation.Z - OldLocation.Z);
							const float MovedDist2DSq = (PawnLocation - OldLocation).SizeSquared2D();
							if (ZMovedDist <= 0.2f * timeTick && MovedDist2DSq <= 4.f * timeTick)
							{
								Velocity.X += 0.25f * GetMaxSpeed() * (FMath::FRand() - 0.5f);
								Velocity.Y += 0.25f * GetMaxSpeed() * (FMath::FRand() - 0.5f);
								Velocity.Z = FMath::Max<float>(JumpZVelocity * 0.25f, 1.f);
								Delta = Velocity * timeTick;
								SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
							}
						}
					}
				}

				// Calculate average velocity based on actual movement after considering collisions
				if (!bJustTeleported)
				{
					// Use average velocity for XY movement (no acceleration except for air control in those axes), but want actual velocity in Z axis
					const float OldVelZ = OldVelocity.Z;
					OldVelocity = (CharacterOwner->GetActorLocation() - OldLocation) / timeTick;
					OldVelocity.Z = OldVelZ;
				}
			}
		}

		if (!HasRootMotion() && !bJustTeleported && MovementMode != MOVE_None)
		{
			// refine the velocity by figuring out the average actual velocity over the tick, and then the final velocity.
			// This particularly corrects for situations where level geometry affected the fall.
			Velocity = (CharacterOwner->GetActorLocation() - OldLocation) / timeTick; //actual average velocity
			if ((Velocity.Z < OldVelocity.Z) || (OldVelocity.Z >= 0.f))
			{
				Velocity = 2.f*Velocity - OldVelocity; //end velocity has 2* accel of avg
			}

			if (Velocity.SizeSquared2D() <= KINDA_SMALL_NUMBER * 10.f)
			{
				Velocity.X = 0.f;
				Velocity.Y = 0.f;
			}

			Velocity = Velocity.ClampMaxSize(GetPhysicsVolume()->TerminalVelocity);
		}
	}
}

void UUTCharacterMovement::NotifyJumpApex()
{
	FindValidLandingSpot(UpdatedComponent->GetComponentLocation());
	Super::NotifyJumpApex();
}

void UUTCharacterMovement::FindValidLandingSpot(const FVector& CapsuleLocation)
{
	// Only try jump assist once, and not while still going up
	if (bJumpAssisted || (Velocity.Z > 0.f))
	{
		return;
	}
	bJumpAssisted = true;
	//UE_LOG(UT, Warning, TEXT("Try Assist"));
	// See if stepping up/forward in acceleration direction would result in valid landing
	FHitResult Result(1.f);
	static const FName LandAssistTraceParamsTag = FName(TEXT("LandAssist"));
	FCollisionQueryParams CapsuleQuery(LandAssistTraceParamsTag, false, CharacterOwner);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(CapsuleQuery, ResponseParam);
	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
	bool bHit = GetWorld()->SweepSingle(Result, PawnLocation, PawnLocation + FVector(0.f, 0.f,LandingStepUp), FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);
	FVector HorizontalStart = bHit ? Result.Location : PawnLocation + FVector(0.f, 0.f, LandingStepUp);
	float ElapsedTime = 0.05f; // FMath::Min(0.05f, remainingTime);
	FVector HorizontalDir = Acceleration.SafeNormal2D() * MaxWalkSpeed * 0.05f;;
	bHit = GetWorld()->SweepSingle(Result, HorizontalStart, HorizontalStart + HorizontalDir, FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);
	FVector LandingStart = bHit ? Result.Location : HorizontalStart + HorizontalDir;
	bHit = GetWorld()->SweepSingle(Result, LandingStart, LandingStart - FVector(0.f, 0.f, LandingStepUp), FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);

	if (IsValidLandingSpot(Result.Location, Result))
	{
		//UE_LOG(UT, Warning, TEXT("ASSIST"));
		bJustTeleported = true;
		if (Cast<AUTCharacter>(CharacterOwner))
		{
			Cast<AUTCharacter>(CharacterOwner)->OnLandingAssist();
		}
		Velocity.Z = LandingAssistBoost; // @TODO FIXMESTEVE what if falling fast need damage, or just skip landing assist
	}
}

/*
void AUTCharacter::Landed(const FHitResult& Hit)
{
	TakeFallingDamage();
	LastHitBy = NULL;

	// adds impulses to vehicles and dynamicSMActors (e.g. PhysicsActors)
	FVector Impulse;
	Impulse.Z = GetVelocity().Z * 4.f; // 4.0f works well for landing on a Scorpion
	if (Cast<ADynamicSMActor>(Hit.GetActor()) != NULL)
	{
		Cast<ADynamicSMActor>(Hit.GetActor())->StaticMeshComponent->AddImpulseAtPosition(Impulse, GetActorLocation());
	}

	if (GetVelocity().Z < -200.f)
	{
		OldZ = GetActorLocation().Z;
		bJustLanded = bUpdateEyeHeight;
	}
	bDodging = false;

	if (!bHidden)
	{
		PlayLandingSound();
	}
	if (CharacterMovement)
	{
		if (GetVelocity().Z < -CharacterMovement->MaxFallSpeed)
		{
			UGameplayStatics::PlaySoundAtLocation(this, FallingDamageLandSound, GetActorLocation());
		}
		else if (GetVelocity().Z < -0.5f * CharacterMovement->MaxFallSpeed)
		{
			UGameplayStatics::PlaySoundAtLocation(this, LandSound, GetActorLocation());
		}
	}

	RecalculateBaseEyeHeight();
}

void AUTPawn::TakeFallingDamage()
{
	if ( CharacterMovement && GetVelocity().Z < -0.5f * CharacterMovement->MaxFallSpeed)
	{
		if (Role == ROLE_Authority)
		{
			if (GetVelocity().Z < -1.f * CharacterMovement->MaxFallSpeed)
			{
				float EffectiveSpeed = GetVelocity().Z;
				if (IsTouchingWaterVolume())
				{
					EffectiveSpeed += 100.f;
				}
				if (EffectiveSpeed < -1.f * CharacterMovement->MaxFallSpeed)
				{
					TakeDamage(-100.f * (EffectiveSpeed + CharacterMovement->MaxFallSpeed) / CharacterMovement->MaxFallSpeed, FDamageEvent(FallingDamageType), NULL, NULL);
				}
			}
		}
	}
}
*/