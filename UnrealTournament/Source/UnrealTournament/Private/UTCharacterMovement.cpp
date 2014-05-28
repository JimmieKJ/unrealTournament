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
	MaxMultiJumpCount = 2;
	bAllowDodgeMultijumps = true;
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
}

bool UUTCharacterMovement::CanDodge()
{
	return (IsMovingOnGround() || IsFalling()) && CanEverJump() && !bWantsToCrouch && (CharacterOwner->GetWorld()->GetTimeSeconds() > DodgeResetTime);
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
		DodgeResetTime = CharacterOwner->GetWorld()->GetTimeSeconds() + WallDodgeResetInterval;
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
	return CharacterOwner && IsMovingOnGround() && !IsCrouching() && (CharacterOwner->GetWorld()->GetTimeSeconds() > SprintStartTime);
}

float UUTCharacterMovement::GetMaxSpeed() const
{
	return CanSprint() ? SprintSpeed : Super::GetMaxSpeed();
}

void UUTCharacterMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	SprintStartTime = CharacterOwner->GetWorld()->GetTimeSeconds() + AutoSprintDelayInterval;
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

void UUTCharacterMovement::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (CharacterOwner && CharacterOwner->NotifyLanded(Hit))
	{
		CharacterOwner->Landed(Hit);
	}
	if (bIsDodging)
	{
		Velocity *= DodgeLandingSpeedFactor;
		DodgeResetTime = CharacterOwner->GetWorld()->GetTimeSeconds() + DodgeResetInterval;
		bIsDodging = false;
	}
	SprintStartTime = CharacterOwner->GetWorld()->GetTimeSeconds() + AutoSprintDelayInterval;

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