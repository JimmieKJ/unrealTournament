// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacterMovement.h"

const float MAX_STEP_SIDE_Z = 0.08f;	// maximum z value for the normal on the vertical side of steps

UUTCharacterMovement::UUTCharacterMovement(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	// @TODO FIXMESTEVE increase this to hour+ for release version
	// MinTimeBetweenTimeStampResets = 3600.f;

	MaxWalkSpeed = 950.f;
	MaxCustomMovementSpeed = MaxWalkSpeed;

	WallDodgeTraceDist = 50.f;
	MinAdditiveDodgeFallSpeed = -2400.f;  // same as UTCharacter->MaxSafeFallSpeed - @TODO FIXMESTEVE probably get rid of this property
	MaxAdditiveDodgeJumpSpeed = 800.f;  
	CurrentMultiJumpCount = 0;
	MaxMultiJumpCount = 1;
	bAllowDodgeMultijumps = false;
	MultiJumpImpulse = 500.f;
	DodgeLandingSpeedFactor = 0.19f;
	DodgeResetInterval = 0.35f;
	WallDodgeResetInterval = 0.2f;
	SprintSpeed = 1250.f;
	SprintAccel = 200.f;
	AutoSprintDelayInterval = 2.f;
	LandingStepUp = 40.f;
	LandingAssistBoost = 380.f;
	CrouchedSpeedMultiplier_DEPRECATED = 0.31f;
	MaxWalkSpeedCrouched = 315.f;
	MaxWallDodges = 99;
	WallDodgeMinNormal = 0.5f;  
	WallDodgeGraceVelocityZ = -600.f;
	AirControl = 0.35f;
	bAllowSlopeDodgeBoost = true;
	MaxStepHeight = 50.f;
	SetWalkableFloorZ(0.695f); 
	MaxAcceleration = 4200.f; 
	BrakingDecelerationWalking = 4800.f;
	GravityScale = 2.2f;
	DodgeImpulseHorizontal = 1350.f;
	DodgeMaxHorizontalVelocity = 1500.f; // DodgeImpulseHorizontal * 1.11
	MaxStepHeight = 51.0f;
	CrouchedHalfHeight = 64.0f;
	SlopeDodgeScaling = 0.93f;
	DodgeRollAcceleration = 2000.f;
	MaxDodgeRollSpeed = 860.f;
	DodgeRollDuration = 0.45f;
	DodgeRollTapInterval = 0.25f;
	RollEndingSpeedFactor = 0.5f;

	MaxSwimSpeed = 450.f;
	Buoyancy = 1.f;

	MaxMultiJumpZSpeed = 280.f;
	JumpZVelocity = 730.f;
	WallDodgeSecondImpulseVertical = 310.f;
	DodgeImpulseVertical = 525.f;
	WallDodgeImpulseHorizontal = 1350.f; 
	WallDodgeImpulseVertical = 440.f; 

	NavAgentProps.bCanCrouch = true;

	// initialization of transient properties
	bIsSprinting = false;
	SprintStartTime = 0.f;
	bJumpAssisted = false;
	DodgeResetTime = 0.f;
	bIsDodging = false;
	bIsDodgeRolling = false;
	DodgeRollTapTime = 0.f;
	DodgeRollEndTime = 0.f;
	CurrentWallDodgeCount = 0;
	bWillDodgeRoll = false;
}

void UUTCharacterMovement::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	if (CharacterOwner == NULL)
	{
		return;
	}
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	Canvas->SetDrawColor(255, 255, 255);
	UFont* RenderFont = GEngine->GetSmallFont();
	FString T = FString::Printf(TEXT(""));
	if (IsInWater())
	{
		T = FString::Printf(TEXT("IN WATER"));
	}
	else if (bIsDodging)
	{
		T = FString::Printf(TEXT("DODGING %f"), CharacterOwner->GetWorld()->GetTimeSeconds());
	}
	else if (bIsDodgeRolling)
	{
		T = FString::Printf(TEXT("DODGE ROLLING"));
	}
	Canvas->DrawText(RenderFont, T, 4.0f, YPos);
	YPos += YL;
	T = FString::Printf(TEXT("AVERAGE SPEED %f"), AvgSpeed);
	Canvas->DrawText(RenderFont, T, 4.0f, YPos);
	YPos += YL;
}

void UUTCharacterMovement::SetGravityScale(float NewGravityScale)
{
	float JumpZScaling = FMath::Sqrt(NewGravityScale / GravityScale);
	MaxMultiJumpZSpeed *= JumpZScaling;
	JumpZVelocity *= JumpZScaling;
	WallDodgeSecondImpulseVertical *= JumpZScaling;
	DodgeImpulseVertical *= JumpZScaling;
	WallDodgeImpulseHorizontal *= JumpZScaling;
	WallDodgeImpulseVertical *= JumpZScaling;
	GravityScale = NewGravityScale;
}

void UUTCharacterMovement::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	if (CharacterOwner)
	{
		OldZ = CharacterOwner->GetActorLocation().Z;
		if (!CharacterOwner->bClientUpdating && CharacterOwner->IsLocallyControlled())
		{
			bIsDodgeRolling = bIsDodgeRolling && (GetCurrentMovementTime() < DodgeRollEndTime);
			bIsSprinting = CanSprint();
			bWillDodgeRoll = (GetCurrentMovementTime() - DodgeRollTapTime < DodgeRollTapInterval);
		}
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AvgSpeed = AvgSpeed * (1.f - 2.f*DeltaTime) + 2.f*DeltaTime * Velocity.Size2D();
}

bool UUTCharacterMovement::ClientUpdatePositionAfterServerUpdate()
{
	if (!HasValidData())
	{
		return false;
	}

	bool bRealSprinting = bIsSprinting;
	bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bIsSprinting = bRealSprinting;
	return bResult;
}

FVector UUTCharacterMovement::GetImpartedMovementBaseVelocity() const
{
	FVector Result = Super::GetImpartedMovementBaseVelocity();

	if (!Result.IsZero())
	{
		// clamp total velocity to GroundSpeed+JumpZ+Imparted total
		float XYSpeed = ((Result.X == 0.f) && (Result.Y == 0.f)) ? 0.f : Result.Size2D();
		float MaxSpeedSq = FMath::Square(MaxWalkSpeed + Result.Size2D()) + FMath::Square(JumpZVelocity + Result.Z);
		if ((Velocity + Result).SizeSquared() > MaxSpeedSq)
		{
			Result = (Velocity + Result).SafeNormal() * FMath::Sqrt(MaxSpeedSq) - Velocity;
		}
	}

	return Result;
}

bool UUTCharacterMovement::CanDodge()
{
	if (GetCurrentMovementTime() < DodgeResetTime)
	{
		//UE_LOG(UT, Warning, TEXT("Failed dodge current move time %f dodge reset time %f"), GetCurrentMovementTime(), DodgeResetTime);
	}
	return (IsMovingOnGround() || IsFalling()) && CanEverJump() && !bWantsToCrouch && ((CharacterOwner != NULL && CharacterOwner->bClientUpdating) || GetCurrentMovementTime() > DodgeResetTime);
}

bool UUTCharacterMovement::PerformDodge(FVector &DodgeDir, FVector &DodgeCross)
{
	if (!HasValidData())
	{
		return false;
	}

	float HorizontalImpulse = DodgeImpulseHorizontal;

	if (IsFalling())
	{
		if (CurrentWallDodgeCount >= MaxWallDodges)
		{
			return false;
		}
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
		if ( (Result.Normal | DodgeDir) < WallDodgeMinNormal )
		{
			// clamp dodge direction based on wall normal
			FVector ForwardDir = (Result.Normal ^ FVector(0.f, 0.f, 1.f)).SafeNormal();
			if ((ForwardDir | DodgeDir) < 0.f)
			{
				ForwardDir *= -1.f;
			}
			DodgeDir = Result.Normal*WallDodgeMinNormal*WallDodgeMinNormal + ForwardDir*(1.f - WallDodgeMinNormal)*(1.f - WallDodgeMinNormal);
			FVector NewDodgeCross = (DodgeDir ^ FVector(0.f, 0.f, 1.f)).SafeNormal();
			DodgeCross = ((NewDodgeCross | DodgeCross) < 0.f) ? -1.f*NewDodgeCross : NewDodgeCross;
		}
		if (!CharacterOwner->bClientUpdating)
		{
			DodgeResetTime = GetCurrentMovementTime() + WallDodgeResetInterval;
			//UE_LOG(UT, Warning, TEXT("Set dodge reset after wall dodge move time %f dodge reset time %f"), GetCurrentMovementTime(), DodgeResetTime);
		}
		HorizontalImpulse = WallDodgeImpulseHorizontal;
		CurrentWallDodgeCount++;
	}
	else if (!GetImpartedMovementBaseVelocity().IsZero())
	{
		// lift jump counts as wall dodge
		CurrentWallDodgeCount++;
	}

	// perform the dodge
	float VelocityZ = Velocity.Z;
	Velocity = HorizontalImpulse*DodgeDir + (Velocity | DodgeCross)*DodgeCross;
	Velocity.Z = 0.f;
	float SpeedXY = FMath::Min(Velocity.Size(), DodgeMaxHorizontalVelocity);
	Velocity = SpeedXY*Velocity.SafeNormal();
	if (!IsFalling())
	{
		Velocity.Z = DodgeImpulseVertical;
	}
	else if ((VelocityZ < MaxAdditiveDodgeJumpSpeed) && (VelocityZ > MinAdditiveDodgeFallSpeed))
	{
		float CurrentWallImpulse = (CurrentWallDodgeCount < 2) ? WallDodgeImpulseVertical : WallDodgeSecondImpulseVertical;

		if ((CurrentWallDodgeCount < 2) && (VelocityZ < 0.f) && (VelocityZ > WallDodgeGraceVelocityZ))
		{
			VelocityZ = 0.f;
		}
		Velocity.Z = FMath::Min(VelocityZ + CurrentWallImpulse, MaxAdditiveDodgeJumpSpeed);
		//UE_LOG(UT, Warning, TEXT("Wall dodge at %f velZ %f"), CharacterOwner->GetWorld()->GetTimeSeconds(), Velocity.Z);
	}
	else
	{
		Velocity.Z = VelocityZ;
	}
	bIsDodging = true;
	bNotifyApex = true;
	CurrentMultiJumpCount++;
	SetMovementMode(MOVE_Falling);
	return true;
}

void UUTCharacterMovement::PerformMovement(float DeltaSeconds)
{
	float RealGroundFriction = GroundFriction;
	if (bIsDodgeRolling)
	{
		GroundFriction = 0.f;
	}
	else if (bWasDodgeRolling)
	{
		Velocity *= RollEndingSpeedFactor;
	}
	bWasDodgeRolling = bIsDodgeRolling;

	bool bSavedWantsToCrouch = bWantsToCrouch;
	bWantsToCrouch = bWantsToCrouch || bIsDodgeRolling;
	bForceMaxAccel = bIsDodgeRolling;

	Super::PerformMovement(DeltaSeconds);
	bWantsToCrouch = bSavedWantsToCrouch;
	GroundFriction = RealGroundFriction;
}

float UUTCharacterMovement::GetMaxAcceleration() const
{
	if (bIsDodgeRolling)
	{
		return DodgeRollAcceleration;
	}
	return (bIsSprinting && (Velocity.SizeSquared() > MaxWalkSpeed*MaxWalkSpeed)) ? SprintAccel : Super::GetMaxAcceleration();
}

bool UUTCharacterMovement::CanSprint() const
{
	if (CharacterOwner && IsMovingOnGround() && !IsCrouching() && (CharacterOwner->bClientUpdating || (GetCurrentMovementTime() > SprintStartTime)))
	{
		// must be movin mostly forward
		FRotator TurnRot(0.f, CharacterOwner->GetActorRotation().Yaw, 0.f);
		FVector X = FRotationMatrix(TurnRot).GetScaledAxis(EAxis::X);
		return ((X | Velocity.SafeNormal()) > 0.6f);
	}
	return false;
}

float UUTCharacterMovement::GetMaxSpeed() const
{
	// ignore standard movement while character is a ragdoll
	if (Cast<AUTCharacter>(CharacterOwner) != NULL && ((AUTCharacter*)CharacterOwner)->IsRagdoll())
	{
		return 0.0f;
	}
	else if (bIsDodgeRolling)
	{
		return MaxDodgeRollSpeed;
	}
	else
	{
		return bIsSprinting ? SprintSpeed : Super::GetMaxSpeed();
	}
}

void UUTCharacterMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (CharacterOwner && !CharacterOwner->bClientUpdating)
	{
		SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

float UUTCharacterMovement::GetCurrentMovementTime() const
{
	return (CharacterOwner && (CharacterOwner->bClientUpdating || ((CharacterOwner->Role == ROLE_Authority) && !CharacterOwner->IsLocallyControlled())))
		? CurrentServerMoveTime
		: CharacterOwner->GetWorld()->GetTimeSeconds();
}

void UUTCharacterMovement::MoveAutonomous
(
float ClientTimeStamp,
float DeltaTime,
uint8 CompressedFlags,
const FVector& NewAccel
)
{
	if (HasValidData())
	{
		//UE_LOG(UT, Warning, TEXT("Set server move time %f"), CurrentServerMoveTime); //MinTimeBetweenTimeStampResets
		if (CurrentServerMoveTime > ClientTimeStamp + 0.5f*MinTimeBetweenTimeStampResets)
		{
			// client timestamp rolled over, so roll over our movement timers
			DodgeResetTime -= MinTimeBetweenTimeStampResets;
			SprintStartTime -= MinTimeBetweenTimeStampResets;
			DodgeRollTapTime -= MinTimeBetweenTimeStampResets;
			DodgeRollEndTime -= MinTimeBetweenTimeStampResets;
		}
		CurrentServerMoveTime = ClientTimeStamp;
	}
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

void UUTCharacterMovement::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (CharacterOwner)
	{
		bIsDodgeRolling = bWillDodgeRoll;
		if (CharacterOwner->NotifyLanded(Hit))
		{
			CharacterOwner->Landed(Hit);
		}
		if (bIsDodging)
		{
			if (bIsDodgeRolling)
			{
				DodgeRollEndTime = GetCurrentMovementTime() + DodgeRollDuration;
				Acceleration = DodgeRollAcceleration * Velocity.SafeNormal2D();
				//UE_LOG(UT, Warning, TEXT("DodgeRoll within %f"), GetCurrentMovementTime() - DodgeRollTapTime);
			}
			else
			{
				Velocity *= DodgeLandingSpeedFactor;
			}
			if (!CharacterOwner->bClientUpdating)
			{
				DodgeResetTime = bIsDodgeRolling ? (DodgeRollEndTime + DodgeResetInterval) : (GetCurrentMovementTime() + DodgeResetInterval);
				//UE_LOG(UT, Warning, TEXT("Set dodge reset after landing move time %f dodge reset time %f"), GetCurrentMovementTime(), DodgeResetTime);
			}
			bIsDodging = false;
		}
		if (!CharacterOwner->bClientUpdating)
		{
			SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
		}
	}
	bJumpAssisted = false;
	CurrentMultiJumpCount = 0;
	CurrentWallDodgeCount = 0;
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
		if (Cast<AUTCharacter>(CharacterOwner) != NULL)
		{
			((AUTCharacter*)CharacterOwner)->PlayJump();
		}
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
		if (CharacterOwner->IsA(AUTCharacter::StaticClass()))
		{
			static FName NAME_MultiJump(TEXT("MultiJump"));
			((AUTCharacter*)CharacterOwner)->InventoryEvent(NAME_MultiJump);
		}
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
	if (bSavedIsSprinting != ((FSavedMove_UTCharacter*)&NewMove)->bSavedIsSprinting)
	{
		return false;
	}
	if ((bSavedIsRolling != ((FSavedMove_UTCharacter*)&NewMove)->bSavedIsRolling) || (bWillDodgeRoll != ((FSavedMove_UTCharacter*)&NewMove)->bWillDodgeRoll))
	{
		return false;
	}
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

	if (bWantsToCrouch)
	{
		Result |= 2;
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
	else if (bSavedIsSprinting)
	{
		Result |= (5 << 2);
	}
	else if (bSavedIsRolling)
	{
		Result |= (6 << 2);
	}
	else if (bWillDodgeRoll)
	{
		Result |= (7 << 2);
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
	bSavedIsSprinting = false;
	bSavedIsRolling = false;
	bWillDodgeRoll = false;
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
		bSavedIsSprinting = UTCharMov->bIsSprinting;
		bSavedIsRolling = UTCharMov->bIsDodgeRolling;
		bWillDodgeRoll = UTCharMov->bWillDodgeRoll;
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

/** @TODO FIXMESTEVE - update super class with this? */
FVector UUTCharacterMovement::ComputeSlideVectorUT(const float DeltaTime, const FVector& InDelta, const float Time, const FVector& Normal, const FHitResult& Hit)
{
	const bool bFalling = IsFalling();
	FVector Delta = InDelta;

	// Don't make impacts on the upper hemisphere feel so much like a capsule
	if (bFalling && Delta.Z > 0.f)
	{
		if (Hit.Normal.Z < KINDA_SMALL_NUMBER)
		{
			float PawnRadius, PawnHalfHeight;
			CharacterOwner->CapsuleComponent->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
			const float UpperHemisphereZ = UpdatedComponent->GetComponentLocation().Z + PawnHalfHeight - PawnRadius;
			if (Hit.ImpactPoint.Z > UpperHemisphereZ + KINDA_SMALL_NUMBER && IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
			{
				Delta = AdjustUpperHemisphereImpact(Delta, Hit);
			}
		}
	}

	FVector Result = UMovementComponent::ComputeSlideVector(Delta, Time, Normal, Hit);

	// prevent boosting up slopes
	if (bFalling && Result.Z > 0.f)
	{
		// @TODO FIXMESTEVE - make this a virtual function in super class so just change this part
		float PawnRadius, PawnHalfHeight;
		CharacterOwner->CapsuleComponent->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
		if (Delta.Z < 0.f && (Hit.ImpactNormal.Z < MAX_STEP_SIDE_Z))
		{
			// We were moving downward, but a slide was going to send us upward. We want to aim
			// straight down for the next move to make sure we get the most upward-facing opposing normal.
			Result = FVector(0.f, 0.f, Delta.Z);
		}
		else if (bAllowSlopeDodgeBoost && (((CharacterOwner->GetActorLocation() - Hit.ImpactPoint).Size2D() > 0.93f * PawnRadius) || (Hit.ImpactNormal.Z > 0.2f))) // @TODO FIXMESTEVE tweak magic numbers
		{
			Result.Z *= SlopeDodgeScaling;
		}
		else
		{
			// Don't move any higher than we originally intended.
			const float ZLimit = Delta.Z * Time;
			if (Result.Z > ZLimit && ZLimit > KINDA_SMALL_NUMBER)
			{
				FVector SlideResult = Result;

				// Rescale the entire vector (not just the Z component) otherwise we change the direction and likely head right back into the impact.
				const float UpPercent = ZLimit / Result.Z;
				Result *= UpPercent;

				// Make remaining portion of original result horizontal and parallel to impact normal.
				const FVector RemainderXY = (SlideResult - Result) * FVector(1.f, 1.f, 0.f);
				const FVector NormalXY = Normal.SafeNormal2D();
				const FVector Adjust = Super::ComputeSlideVector(RemainderXY, 1.f, NormalXY, Hit);
				Result += Adjust;
			}
		}
	}

	return Result;
}

bool UUTCharacterMovement::CanCrouchInCurrentState() const
{
	return CanEverCrouch() && IsMovingOnGround();
}

/** @TODO FIXMESTEVE - physfalling copied from base version and edited.  At some point should probably add some hooks to base version and use those instead. */
void UUTCharacterMovement::PhysFalling(float deltaTime, int32 Iterations)
{
	// Bound final 2d portion of velocity
	const float Speed2d = Velocity.Size2D();
	const float BoundSpeed = FMath::Max(Speed2d, GetMaxSpeed() * AnalogInputModifier);

	//bound acceleration, falling object has minimal ability to impact acceleration
	FVector FallAcceleration = Acceleration;
	FallAcceleration.Z = 0.f;
	bool bSkipLandingAssist = true;
	FHitResult Hit(1.f);
	if (!HasRootMotion())
	{
		// test for slope to avoid using air control to climb walls 
		float TickAirControl = AirControl;
		if (TickAirControl > 0.0f && FallAcceleration.SizeSquared() > 0.f)
		{
			const float TestWalkTime = FMath::Max(deltaTime, 0.05f);
			const FVector TestWalk = ((TickAirControl * GetMaxAcceleration() * FallAcceleration.SafeNormal() + FVector(0.f, 0.f, GetGravityZ())) * TestWalkTime + Velocity) * TestWalkTime;
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

		float MaxAccel = GetMaxAcceleration() * TickAirControl;

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
				ProcessLanded(Hit, remainingTime, Iterations);
				return;
			}
			else
			{
				// See if we can convert a normally invalid landing spot (based on the hit result) to a usable one.
				if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(timeTick, Adjusted, Hit))
				{
					const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
					FFindFloorResult FloorResult;
					FindFloor(PawnLocation, FloorResult, false);
					if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
					{
						remainingTime += timeTick * (1.f - Hit.Time);
						ProcessLanded(FloorResult.HitResult, remainingTime, Iterations);
						return;
					}
				}
				if (!bSkipLandingAssist)
				{
					FindValidLandingSpot(UpdatedComponent->GetComponentLocation());
				}
				HandleImpact(Hit, deltaTime, Adjusted);

				// If we've changed physics mode, abort.
				if (!HasValidData() || !IsFalling())
				{
					return;
				}

				const float FirstHitPercent = Hit.Time;
				const FVector OldHitNormal = Hit.Normal;
				const FVector OldHitImpactNormal = Hit.ImpactNormal;
				FVector Delta = ComputeSlideVectorUT(timeTick * (1.f - Hit.Time), Adjusted, 1.f - Hit.Time, OldHitNormal, Hit);

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

						HandleImpact(Hit, timeTick * (1.f - FirstHitPercent), Delta);

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

bool UUTCharacterMovement::ShouldCheckForValidLandingSpot(const float DeltaTime, const FVector& Delta, const FHitResult& Hit) const
{
	// See if we hit an edge of a surface on the lower portion of the capsule.
	// In this case the normal will not equal the impact normal, and a downward sweep may find a walkable surface on top of the edge.
	if (Hit.Normal.Z > KINDA_SMALL_NUMBER && !Hit.Normal.Equals(Hit.ImpactNormal))
	{
		const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
		if (IsWithinEdgeTolerance(PawnLocation, Hit.ImpactPoint, CharacterOwner->CapsuleComponent->GetScaledCapsuleRadius()))
		{
			return true;
		}
	}

	return false;
}


void UUTCharacterMovement::NotifyJumpApex()
{
	if (Cast<AUTCharacter>(CharacterOwner))
	{
		Cast<AUTCharacter>(CharacterOwner)->NotifyJumpApex();
	}
	FindValidLandingSpot(UpdatedComponent->GetComponentLocation());
	Super::NotifyJumpApex();
}

void UUTCharacterMovement::FindValidLandingSpot(const FVector& CapsuleLocation)
{
	// Only try jump assist once, and not while still going up, and not if falling too fast
	if (bJumpAssisted || (Velocity.Z > 0.f) || (Cast<AUTCharacter>(CharacterOwner) != NULL && Velocity.Z < -1.f*((AUTCharacter*)CharacterOwner)->MaxSafeFallSpeed))
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
		Velocity.Z = LandingAssistBoost; 
		//UE_LOG(UT, Warning, TEXT("LANDING ASSIST BOOST"));
	}
}
