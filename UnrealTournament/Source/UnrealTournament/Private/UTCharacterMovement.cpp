// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacterMovement.h"
#include "GameFramework/GameNetworkManager.h"
#include "UTLift.h"
#include "UTReachSpec_Lift.h"
#include "UTWaterVolume.h"
#include "UTBot.h"

const float MAX_STEP_SIDE_Z = 0.08f;	// maximum z value for the normal on the vertical side of steps

UUTCharacterMovement::UUTCharacterMovement(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Reduce for testing of transitions
	// MinTimeBetweenTimeStampResets = 5.f;
	MinTimeBetweenTimeStampResets = 10000.f;

	MaxWalkSpeed = 940.f;
	MaxCustomMovementSpeed = MaxWalkSpeed;

	WallDodgeTraceDist = 50.f;
	MinAdditiveDodgeFallSpeed = -5000.f;  
	MaxAdditiveDodgeJumpSpeed = 700.f;  
	MaxMultiJumpCount = 0;
	bAllowDodgeMultijumps = false;
	bAllowJumpMultijumps = true;
	MultiJumpImpulse = 600.f;
	DodgeJumpImpulse = 600.f;
	DodgeLandingSpeedFactor = 0.19f;
	DodgeJumpLandingSpeedFactor = 0.19f;
	DodgeResetInterval = 0.35f;
	DodgeJumpResetInterval = 0.35f;
	WallDodgeResetInterval = 0.2f;
	SprintSpeed = 1230.f;
	SprintAccel = 200.f;
	SprintMaxWallNormal = -0.7f;
	AutoSprintDelayInterval = 1.5f;
	LandingStepUp = 40.f;
	LandingAssistBoost = 430.f;
	CrouchedSpeedMultiplier_DEPRECATED = 0.31f;
	MaxWalkSpeedCrouched = 315.f;
	MaxWallDodges = 99;
	WallDodgeMinNormal = 0.5f; 
	MaxConsecutiveWallDodgeDP = 0.97f;
	WallDodgeGraceVelocityZ = -2400.f;
	AirControl = 0.4f;
	MultiJumpAirControl = 0.4f;
	bAllowSlopeDodgeBoost = true;
	SetWalkableFloorZ(0.695f); 
	MaxAcceleration = 6600.f; 
	MaxFallingAcceleration = 4600.f;
	BrakingDecelerationWalking = 500.f;
	BrakingDecelerationFalling = 0.f;
	BrakingDecelerationSwimming = 300.f;
	GroundFriction = 5.f;
	GravityScale = 1.f;
	DodgeImpulseHorizontal = 1350.f;
	DodgeMaxHorizontalVelocity = 1500.f; // DodgeImpulseHorizontal * 1.11
	MaxStepHeight = 51.0f;
	NavAgentProps.AgentStepHeight = MaxStepHeight; // warning: must be manually mirrored, won't be set automatically
	CrouchedHalfHeight = 55.0f;
	FloorSlideHalfHeight = 55.f;
	SlopeDodgeScaling = 0.93f;

	FloorSlideAcceleration = 2000.f;
	MaxFloorSlideSpeed = 920.f;
	FloorSlideDuration = 0.45f;
	FloorSlideBonusTapInterval = 0.17f;
	FloorSlideEarliestZ = -100.f;
	FloorSlideEndingSpeedFactor = 0.5f;
	FallingDamageRollReduction = 6.f;

	MaxSwimSpeed = 1000.f;
	MaxWaterSpeed = 450.f; 
	Buoyancy = 1.f;
	SwimmingWallPushImpulse = 730.f;

	MaxMultiJumpZSpeed = 280.f;
	JumpZVelocity = 730.f;
	WallDodgeSecondImpulseVertical = 320.f;
	DodgeImpulseVertical = 525.f;
	WallDodgeImpulseHorizontal = 1350.f; 
	WallDodgeImpulseVertical = 470.f; 

	MaxSlideFallZ = -200.f;
	SlideGravityScaling = 0.2f;
	MinWallSlideSpeed = 500.f;
	MaxSlideAccelNormal = 0.f;

	NavAgentProps.bCanCrouch = true;

	// initialization of transient properties
	bIsSprinting = false;						
	SprintStartTime = 0.f;						
	bJumpAssisted = false;					
	DodgeResetTime = 0.f;					
	bIsDodging = false;					
	bIsFloorSliding = false;				
	FloorSlideTapTime = 0.f;					
	FloorSlideEndTime = 0.f;					
	CurrentMultiJumpCount = 0;	
	bExplicitJump = false;
	CurrentWallDodgeCount = 0;				
	bWantsFloorSlide = false;				
	LastCheckedAgainstWall = 0.f;
	bIsSettingUpFirstReplayMove = false;

	EasyImpactImpulse = 1100.f;
	EasyImpactDamage = 25;
	FullImpactImpulse = 1600.f;
	FullImpactDamage = 40;
	ImpactMaxHorizontalVelocity = 1500.f;
	ImpactMaxVerticalFactor = 1.f;
	MaxUndampedImpulse = 2000.f;

	OutofWaterZ = 700.f;
	JumpOutOfWaterPitch = -90.f;
	bFallingInWater = false;

	MaxPositionErrorSquared = 5.f;
	LastClientAdjustmentTime = -1.f;
	LastGoodMoveAckTime = -1.f;
	MinTimeBetweenClientAdjustments = 0.1f;
	bLargeCorrection = false;
	LargeCorrectionThreshold = 15.f;
}

// @todo UE4 - handle lift moving up and down through encroachment
void UUTCharacterMovement::UpdateBasedMovement(float DeltaSeconds)
{
	Super::UpdateBasedMovement(DeltaSeconds);

	if (HasValidData())
	{
		// check for bot lift jump
		AUTBot* B = Cast<AUTBot>(CharacterOwner->Controller);
		if (B != NULL && CharacterOwner->CanJump())
		{
			FVector LiftVelocity = (CharacterOwner->GetMovementBase() != NULL) ? CharacterOwner->GetMovementBase()->GetComponentVelocity() : FVector::ZeroVector;
			if (!LiftVelocity.IsZero())
			{
				UUTReachSpec_Lift* LiftPath = NULL;
				int32 ExitRouteIndex = INDEX_NONE;
				if ((B->GetCurrentPath().ReachFlags & R_JUMP) && B->GetCurrentPath().Spec.IsValid())
				{
					LiftPath = Cast<UUTReachSpec_Lift>(B->GetCurrentPath().Spec.Get());
				}
				// see if bot's next path is a lift jump (need to check this for fast moving lifts, because CurrentPath won't change until bot reaches lift center which in certain cases might be too late)
				else if (B->GetMoveTarget().Node != NULL)
				{
					for (int32 i = 0; i < B->RouteCache.Num() - 1; i++)
					{
						if (B->RouteCache[i] == B->GetMoveTarget())
						{
							int32 LinkIndex = B->GetMoveTarget().Node->GetBestLinkTo(B->GetMoveTarget().TargetPoly, B->RouteCache[i + 1], CharacterOwner, CharacterOwner->GetNavAgentPropertiesRef(), GetUTNavData(GetWorld()));
							if (LinkIndex != INDEX_NONE && (B->GetMoveTarget().Node->Paths[LinkIndex].ReachFlags & R_JUMP) && B->GetMoveTarget().Node->Paths[LinkIndex].Spec.IsValid())
							{
								LiftPath = Cast<UUTReachSpec_Lift>(B->GetMoveTarget().Node->Paths[LinkIndex].Spec.Get());
								ExitRouteIndex = i + 1;
							}
						}
					}
				}
				if (LiftPath != NULL)
				{
					const FVector PawnLoc = CharacterOwner->GetActorLocation();
					float XYSize = (LiftPath->LiftExitLoc - PawnLoc).Size2D();
					float Time = XYSize / MaxWalkSpeed;
					// test with slightly less than actual velocity to provide some room for error and so bots aren't perfect all the time
					// TODO: maybe also delay more if lift is known to have significant travel time remaining?
					if (PawnLoc.Z + (LiftVelocity.Z + JumpZVelocity * 0.9f) * Time + 0.5f * GetGravityZ() * FMath::Square<float>(Time) >= LiftPath->LiftExitLoc.Z)
					{
						// jump!
						FVector DesiredVel2D;
						if (B->FindBestJumpVelocityXY(DesiredVel2D, CharacterOwner->GetActorLocation(), LiftPath->LiftExitLoc, LiftVelocity.Z + JumpZVelocity, GetGravityZ(), CharacterOwner->GetSimpleCollisionHalfHeight()))
						{
							Velocity = FVector(DesiredVel2D.X, DesiredVel2D.Y, 0.0f).GetClampedToMaxSize2D(MaxWalkSpeed);
						}
						else
						{
							Velocity = (LiftPath->LiftExitLoc - PawnLoc).GetSafeNormal2D() * MaxWalkSpeed;
						}
						DoJump(false);
						// redirect bot to next point on route if necessary
						if (ExitRouteIndex != INDEX_NONE)
						{
							TArray<FComponentBasedPosition> MovePoints;
							new(MovePoints) FComponentBasedPosition(LiftPath->LiftExitLoc);
							B->SetMoveTarget(B->RouteCache[ExitRouteIndex], MovePoints);
						}
						else if (B->GetMoveBasedPosition().Base != NULL && B->GetMoveBasedPosition().Base->GetOwner() == LiftPath->Lift)
						{
							// result of bug where bot didn't use an entry path to get on the lift due to navmesh structure, see UUTReachSpec_Lift::GetMovePoints()
							B->SetAdjustLoc(LiftPath->LiftExitLoc);
							B->MoveTimer = -1.0f;
						}
					}
				}
			}
		}
	}
}

void UUTCharacterMovement::OnUnableToFollowBaseMove(const FVector& DeltaPosition, const FVector& OldLocation, const FHitResult& MoveOnBaseHit)
{
	UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();

	// @TODO FIXMESTEVE handle case where we should adjust player position so he doesn't encroach, but don't need to make lift return
	// Test if lift is moving up/sideways, otherwise ignore this (may need more sophisticated test)
	if (MovementBase && Cast<AUTLift>(MovementBase->GetOwner()) && (MovementBase->GetOwner()->GetVelocity().Z >= 0.f))// && UpdatedComponent->IsOverlappingComponent(MovementBase))
	{
		Cast<AUTLift>(MovementBase->GetOwner())->OnEncroachActor(CharacterOwner);
	}

	// make sure bots on lift move to center in case they are causing the lift to fail by hitting their head on the sides
	AUTBot* B = Cast<AUTBot>(CharacterOwner->Controller);
	if (B != NULL)
	{
		UUTReachSpec_Lift* LiftPath = Cast<UUTReachSpec_Lift>(B->GetCurrentPath().Spec.Get());
		if (LiftPath != NULL)
		{
			B->SetAdjustLoc(LiftPath->LiftCenter + FVector(0.0f, 0.0f, B->GetCharacter()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
		}
	}
}

void UUTCharacterMovement::ApplyImpactVelocity(FVector JumpDir, bool bIsFullImpactImpulse)
{
	// provide scaled boost in facing direction, clamped to ImpactMaxHorizontalVelocity and ImpactMaxVerticalVelocity
	// @TODO FIXMESTEVE should use AddDampedImpulse()?
	float ImpulseMag = bIsFullImpactImpulse ? FullImpactImpulse : EasyImpactImpulse;
	FVector NewVelocity = Velocity + JumpDir * ImpulseMag;
	if (NewVelocity.Size2D() > ImpactMaxHorizontalVelocity)
	{
		float VelZ = NewVelocity.Z;
		NewVelocity = NewVelocity.GetSafeNormal2D() * ImpactMaxHorizontalVelocity;
		NewVelocity.Z = VelZ;
	}
	NewVelocity.Z = FMath::Min(NewVelocity.Z, ImpactMaxVerticalFactor*ImpulseMag);
	Velocity = NewVelocity;
	SetMovementMode(MOVE_Falling);
	bNotifyApex = true;
	NeedsClientAdjustment();
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
	else if (bIsFloorSliding)
	{
		T = FString::Printf(TEXT("DODGE ROLLING"));
	}
	Canvas->DrawText(RenderFont, T, 4.0f, YPos);
	YPos += YL;
	T = FString::Printf(TEXT("AVERAGE SPEED %f"), AvgSpeed);
	Canvas->DrawText(RenderFont, T, 4.0f, YPos);
	YPos += YL;
}

float UUTCharacterMovement::UpdateTimeStampAndDeltaTime(float DeltaTime, FNetworkPredictionData_Client_Character* ClientData)
{
	float UnModifiedTimeStamp = ClientData->CurrentTimeStamp + DeltaTime;
	DeltaTime = ClientData->UpdateTimeStampAndDeltaTime(DeltaTime, *CharacterOwner, *this);
	if (ClientData->CurrentTimeStamp < UnModifiedTimeStamp)
	{
		// client timestamp rolled over, so roll over our movement timers
		AdjustMovementTimers(UnModifiedTimeStamp - ClientData->CurrentTimeStamp);
	}
	return DeltaTime;
}

void UUTCharacterMovement::AdjustMovementTimers(float Adjustment)
{
	//UE_LOG(UTNet, Warning, TEXT("+++++++ROLLOVER time %f"), CurrentServerMoveTime); //MinTimeBetweenTimeStampResets
	DodgeResetTime -= Adjustment;
	SprintStartTime -= Adjustment;
	FloorSlideTapTime -= Adjustment;
	FloorSlideEndTime -= Adjustment;
}

void UUTCharacterMovement::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	AUTCharacter* UTOwner = Cast<AUTCharacter>(CharacterOwner);
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	bool bOwnerIsRagdoll = UTOwner && UTOwner->IsRagdoll();
	if (bOwnerIsRagdoll)
	{
		// ignore jump/slide key presses this frame since the character is in ragdoll and they don't apply
		UTOwner->bPressedJump = false;
		bPressedSlide = false;
		if (!UTOwner->GetController())
		{
			return;
		}
	}

	const FVector InputVector = ConsumeInputVector();
	if (!HasValidData() || ShouldSkipUpdate(DeltaTime) || UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	if (CharacterOwner->Role > ROLE_SimulatedProxy)
	{
		if (CharacterOwner->Role == ROLE_Authority)
		{
			// Check we are still in the world, and stop simulating if not.
			const bool bStillInWorld = (bCheatFlying || CharacterOwner->CheckStillInWorld());
			if (!bStillInWorld || !HasValidData())
			{
				return;
			}
		}

		// If we are a client we might have received an update from the server.
		const bool bIsClient = (GetNetMode() == NM_Client && CharacterOwner->Role == ROLE_AutonomousProxy);
		if (bIsClient && !bOwnerIsRagdoll)
		{
			ClientUpdatePositionAfterServerUpdate();
		}

		// Allow root motion to move characters that have no controller.
		if (CharacterOwner->IsLocallyControlled() || bRunPhysicsWithNoController || (!CharacterOwner->Controller && CharacterOwner->IsPlayingRootMotion()))
		{
			FNetworkPredictionData_Client_Character* ClientData = ((CharacterOwner->Role < ROLE_Authority) && (GetNetMode() == NM_Client)) ? GetPredictionData_Client_Character() : NULL;
			if (ClientData)
			{
				// Update our delta time for physics simulation.
				DeltaTime = UpdateTimeStampAndDeltaTime(DeltaTime, ClientData);
				CurrentServerMoveTime = ClientData->CurrentTimeStamp;
			}
			else
			{
				CurrentServerMoveTime = GetWorld()->GetTimeSeconds();
			}
			//UE_LOG(UTNet, Warning, TEXT("Correction COMPLETE velocity %f %f %f"), Velocity.X, Velocity.Y, Velocity.Z);
			// We need to check the jump state before adjusting input acceleration, to minimize latency
			// and to make sure acceleration respects our potentially new falling state.
			CharacterOwner->CheckJumpInput(DeltaTime);

			// apply input to acceleration
			Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector));
			AnalogInputModifier = ComputeAnalogInputModifier();

			if ((CharacterOwner->Role == ROLE_Authority) && !bOwnerIsRagdoll)
			{
				PerformMovement(DeltaTime);
			}
			else if (bIsClient)
			{
				ReplicateMoveToServer(DeltaTime, Acceleration);
			}
		}
		else if ((CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy) && !bOwnerIsRagdoll)
		{
			// Server ticking for remote client.
			// Between net updates from the client we need to update position if based on another object,
			// otherwise the object will move on intermediate frames and we won't follow it.
			MaybeUpdateBasedMovement(DeltaTime);
			SaveBaseLocation();
		}
		else if (!CharacterOwner->Controller && (CharacterOwner->Role == ROLE_Authority) && !bOwnerIsRagdoll)
		{
			// still update forces
			ApplyAccumulatedForces(DeltaTime);
			PerformMovement(DeltaTime);
		}
	}
	else if (!bOwnerIsRagdoll && CharacterOwner->Role == ROLE_SimulatedProxy)
	{
		AdjustProxyCapsuleSize();
		SimulatedTick(DeltaTime);
	}
	if (bEnablePhysicsInteraction && !bOwnerIsRagdoll)
	{
		if (CurrentFloor.HitResult.IsValidBlockingHit())
		{
			// Apply downwards force when walking on top of physics objects
			if (UPrimitiveComponent* BaseComp = CurrentFloor.HitResult.GetComponent())
			{
				if (StandingDownwardForceScale != 0.f && BaseComp->IsAnySimulatingPhysics())
				{
					const float GravZ = GetGravityZ();
					const FVector ForceLocation = CurrentFloor.HitResult.ImpactPoint;
					BaseComp->AddForceAtLocation(FVector(0.f, 0.f, GravZ * Mass * StandingDownwardForceScale), ForceLocation, CurrentFloor.HitResult.BoneName);
				}
			}
		}
	}

	if (bOwnerIsRagdoll)
	{
		// ignore jump/slide key presses this frame since the character is in ragdoll and they don't apply
		UTOwner->bPressedJump = false;
		bPressedSlide = false;
	}
	AvgSpeed = AvgSpeed * (1.f - 2.f*DeltaTime) + 2.f*DeltaTime * Velocity.Size2D();
	if (CharacterOwner != NULL)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(CharacterOwner->Controller);
		if (PC != NULL && PC->PlayerInput != NULL)
		{
			PC->ApplyDeferredFireInputs();
		}
	}
}

void UUTCharacterMovement::ClearPendingImpulse()
{
	PendingImpulseToApply = FVector(0.f);
}

void UUTCharacterMovement::AddDampedImpulse(FVector Impulse, bool bSelfInflicted)
{
	if (HasValidData() && !Impulse.IsZero())
	{
		// handle scaling by mass
		FVector FinalImpulse = Impulse;
		if (Mass > SMALL_NUMBER)
		{
			FinalImpulse = FinalImpulse / Mass;
		}

		// dampen impulse if already traveling fast.  Shouldn't affect 1st rocket, but should affect 2 and 3
		// First dampen XY component that is in the direction of the current PendingVelocity
		float FinalImpulseZ = FinalImpulse.Z;
		FinalImpulse.Z = 0.f;
		FVector PendingVelocity = Velocity + PendingImpulseToApply;
		FVector PendingVelocityDir = PendingVelocity.GetSafeNormal();
		FVector AdditiveImpulse = PendingVelocityDir * (PendingVelocityDir | FinalImpulse);
		FVector OrthogonalImpulse = FinalImpulse - AdditiveImpulse;
		FVector ResultVelocity = PendingVelocity + AdditiveImpulse;
		float CurrentXYSpeed = PendingVelocity.Size2D();
		float ResultXYSpeed = ResultVelocity.Size2D();
		float XYDelta = ResultXYSpeed - CurrentXYSpeed;
		if (XYDelta > 0.f)
		{
			// reduce additive impulse further if current speed is already beyond dodge speed (implying this is 2nd or further impulse applied)
			float AboveDodgeFactor = CurrentXYSpeed / DodgeImpulseHorizontal;
			if (AboveDodgeFactor > 1.0f)
			{
				FinalImpulse = AdditiveImpulse / AboveDodgeFactor + OrthogonalImpulse;
			}

			float PctBelowRun = FMath::Clamp((MaxWalkSpeed - CurrentXYSpeed)/XYDelta, 0.f, 1.f);
			float PctBelowDodge = FMath::Clamp((DodgeImpulseHorizontal - CurrentXYSpeed)/XYDelta, 0.f ,1.f);
			float PctAboveDodge = FMath::Max(0.f, 1.f - PctBelowDodge);
			PctBelowDodge = FMath::Max(0.f, PctBelowDodge - PctBelowRun);
			FinalImpulse *= (PctBelowRun + PctBelowDodge + FMath::Max(0.5f, 1.f - PctAboveDodge)*PctAboveDodge);
		}
		FinalImpulse.Z = FinalImpulseZ;

		// Now for Z component
		float DampingThreshold = bSelfInflicted ? MaxUndampedImpulse : MaxAdditiveDodgeJumpSpeed;
		if ((FinalImpulse.Z > 0.f) && (FinalImpulse.Z + PendingVelocity.Z > DampingThreshold))
		{
			float PctBelowBoost = FMath::Clamp((DampingThreshold - PendingVelocity.Z) / FinalImpulse.Z, 0.f, 1.f);
			FinalImpulse.Z *= (PctBelowBoost + (1.f - PctBelowBoost)*FMath::Max(0.5f, PctBelowBoost));
		}

		PendingImpulseToApply += FinalImpulse;
		NeedsClientAdjustment();
	}
}

FVector UUTCharacterMovement::GetImpartedMovementBaseVelocity() const
{
	FVector Result = Super::GetImpartedMovementBaseVelocity();

	if (!Result.IsZero())
	{
		// clamp total velocity to GroundSpeed+JumpZ+Imparted total TODO SEPARATE XY and Z
		float XYSpeed = ((Result.X == 0.f) && (Result.Y == 0.f)) ? 0.f : Result.Size2D();
		float MaxSpeedSq = FMath::Square(MaxWalkSpeed + Result.Size2D()) + FMath::Square(JumpZVelocity + Result.Z);
		if ((Velocity + Result).SizeSquared() > MaxSpeedSq)
		{
			Result = (Velocity + Result).GetSafeNormal() * FMath::Sqrt(MaxSpeedSq) - Velocity;
		}
		Result.Z = FMath::Max(Result.Z, 0.f);
	}

	return Result;
}

bool UUTCharacterMovement::CanDodge()
{
/*	if (GetCurrentMovementTime() < DodgeResetTime)
	{
		UE_LOG(UTNet, Warning, TEXT("Failed dodge current move time %f dodge reset time %f"), GetCurrentMovementTime(), DodgeResetTime);
	}
	else
	{
		UE_LOG(UTNet, Warning, TEXT("SUCCEEDED candodge current move time %f dodge reset time %f"), GetCurrentMovementTime(), DodgeResetTime);
	}
*/
	return !bIsFloorSliding && CanEverJump() && (GetCurrentMovementTime() > DodgeResetTime);
}

bool UUTCharacterMovement::CanJump()
{
	return (IsMovingOnGround() || CanMultiJump()) && CanEverJump() && !bWantsToCrouch && !bIsFloorSliding;
}

void UUTCharacterMovement::PerformWaterJump()
{
	if (!HasValidData())
	{
		return;
	}

	float PawnCapsuleRadius, PawnCapsuleHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnCapsuleRadius, PawnCapsuleHalfHeight);
	FVector TraceStart = CharacterOwner->GetActorLocation();
	TraceStart.Z = TraceStart.Z - PawnCapsuleHalfHeight + PawnCapsuleRadius;
	FVector TraceEnd = TraceStart - FVector(0.f, 0.f, WallDodgeTraceDist);

	static const FName DodgeTag = FName(TEXT("Dodge"));
	FCollisionQueryParams QueryParams(DodgeTag, false, CharacterOwner);
	FHitResult Result;
	const bool bBlockingHit = GetWorld()->SweepSingleByChannel(Result, TraceStart, TraceEnd, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(), FCollisionShape::MakeSphere(PawnCapsuleRadius), QueryParams);
	if (!bBlockingHit)
	{
		return;
	}
	DodgeResetTime = GetCurrentMovementTime() + WallDodgeResetInterval;
	if (!CharacterOwner->bClientUpdating)
	{
		//UE_LOG(UTNet, Warning, TEXT("Set dodge reset after wall dodge move time %f dodge reset time %f"), GetCurrentMovementTime(), DodgeResetTime);

		// @TODO FIXMESTEVE - character should be responsible for effects, should have blueprint event too
		AUTCharacter* UTCharacterOwner = Cast<AUTCharacter>(CharacterOwner);
		if (UTCharacterOwner)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), UTCharacterOwner->SwimPushSound, UTCharacterOwner, SRT_AllButOwner);
		}
	}
	LastWallDodgeNormal = Result.ImpactNormal;
	Velocity.Z = FMath::Max(Velocity.Z, SwimmingWallPushImpulse);
}

bool UUTCharacterMovement::PerformDodge(FVector &DodgeDir, FVector &DodgeCross)
{
	if (!HasValidData())
	{
		return false;
	}
/*	
	FVector Loc = CharacterOwner->GetActorLocation();
	UE_LOG(UTNet, Warning, TEXT("Perform dodge at %f loc %f %f %f vel %f %f %f dodgedir %f %f %f from yaw %f"), GetCurrentSynchTime(), Loc.X, Loc.Y, Loc.Z, Velocity.X, Velocity.Y, Velocity.Z, DodgeDir.X, DodgeDir.Y, DodgeDir.Z, CharacterOwner->GetActorRotation().Yaw);
*/
	float HorizontalImpulse = DodgeImpulseHorizontal;
	bool bIsLowGrav = (GetGravityZ() > UPhysicsSettings::Get()->DefaultGravityZ);
	AUTCharacter* UTCharOwner = Cast<AUTCharacter>(CharacterOwner);
	if (UTCharOwner)
	{
		bIsLowGrav = !UTCharOwner->bApplyWallSlide && bIsLowGrav;
	}
	NeedsClientAdjustment();

	if (!IsMovingOnGround())
	{
		if (IsFalling() && (CurrentWallDodgeCount >= MaxWallDodges))
		{
			//UE_LOG(UTNet, Warning, TEXT("Exceeded max wall dodges"));
			return false;
		}
		// if falling/swimming, check if can perform wall dodge
		FVector TraceEnd = -1.f*DodgeDir;
		float PawnCapsuleRadius, PawnCapsuleHalfHeight;
		CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnCapsuleRadius, PawnCapsuleHalfHeight);
		float TraceBoxSize = FMath::Min(0.25f*PawnCapsuleHalfHeight, 0.7f*PawnCapsuleRadius);
		FVector TraceStart = CharacterOwner->GetActorLocation();
		TraceStart.Z -= 0.5f*TraceBoxSize;
		TraceEnd = TraceStart - (WallDodgeTraceDist+PawnCapsuleRadius-0.5f*TraceBoxSize)*DodgeDir;

		static const FName DodgeTag = FName(TEXT("Dodge"));
		FCollisionQueryParams QueryParams(DodgeTag, false, CharacterOwner);
		FHitResult Result;
		const bool bBlockingHit = GetWorld()->SweepSingleByChannel(Result, TraceStart, TraceEnd, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(), FCollisionShape::MakeSphere(TraceBoxSize), QueryParams);
		if (!bBlockingHit || (!IsSwimming() && (CurrentWallDodgeCount > 0) && !bIsLowGrav && ((Result.ImpactNormal | LastWallDodgeNormal) > MaxConsecutiveWallDodgeDP)))
		{
			//UE_LOG(UTNet, Warning, TEXT("No wall to dodge"));
			return false;
		}
		if ((Result.ImpactNormal | DodgeDir) < WallDodgeMinNormal)
		{
			// clamp dodge direction based on wall normal
			FVector ForwardDir = (Result.ImpactNormal ^ FVector(0.f, 0.f, 1.f)).GetSafeNormal();
			if ((ForwardDir | DodgeDir) < 0.f)
			{
				ForwardDir *= -1.f;
			}
			DodgeDir = Result.ImpactNormal*WallDodgeMinNormal*WallDodgeMinNormal + ForwardDir*(1.f - WallDodgeMinNormal*WallDodgeMinNormal);
			DodgeDir = DodgeDir.GetSafeNormal();
			FVector NewDodgeCross = (DodgeDir ^ FVector(0.f, 0.f, 1.f)).GetSafeNormal();
			DodgeCross = ((NewDodgeCross | DodgeCross) < 0.f) ? -1.f*NewDodgeCross : NewDodgeCross;
		}
		DodgeResetTime = GetCurrentMovementTime() + WallDodgeResetInterval;
		HorizontalImpulse = IsSwimming() ? SwimmingWallPushImpulse : WallDodgeImpulseHorizontal;
		CurrentWallDodgeCount++;
		LastWallDodgeNormal = Result.ImpactNormal;
	}
	else if (!GetImpartedMovementBaseVelocity().IsZero())
	{
		// lift jump counts as wall dodge
		CurrentWallDodgeCount++;
		LastWallDodgeNormal = FVector(0.f, 0.f, 1.f);
		DodgeResetTime = GetCurrentMovementTime() + WallDodgeResetInterval;
	}

	// perform the dodge
	//UE_LOG(UTNet, Warning, TEXT("Imparted velocity"));
	float VelocityZ = Velocity.Z;
	Velocity = HorizontalImpulse*DodgeDir + (Velocity | DodgeCross)*DodgeCross;
	Velocity.Z = 0.f;
	float SpeedXY = FMath::Min(Velocity.Size(), DodgeMaxHorizontalVelocity);
	Velocity = SpeedXY*Velocity.GetSafeNormal();
	if (IsMovingOnGround())
	{
		Velocity.Z = DodgeImpulseVertical;
	}
	else if (!IsSwimming() && (VelocityZ < MaxAdditiveDodgeJumpSpeed) && (VelocityZ > MinAdditiveDodgeFallSpeed))
	{
		float CurrentWallImpulse = (CurrentWallDodgeCount < 2) ? WallDodgeImpulseVertical : WallDodgeSecondImpulseVertical;

		if (!bIsLowGrav && (CurrentWallDodgeCount > 1))
		{
			VelocityZ = FMath::Min(0.f, VelocityZ);
		}
		else if ((VelocityZ < 0.f) && (VelocityZ > WallDodgeGraceVelocityZ))
		{
			if (Velocity.Z < -1.f * JumpZVelocity)
			{
				CurrentWallImpulse = FMath::Max(WallDodgeSecondImpulseVertical, WallDodgeImpulseVertical + Velocity.Z + JumpZVelocity);
			}
			// allowing dodge with loss of downward velocity is no free lunch for falling damage
			FHitResult Hit(1.f);
			Hit.ImpactNormal = FVector(0.f, 0.f, 1.f);
			Hit.Normal = Hit.ImpactNormal;
			Cast<AUTCharacter>(CharacterOwner)->TakeFallingDamage(Hit, VelocityZ - CurrentWallImpulse);
			VelocityZ = 0.f;
		}
		Velocity.Z = FMath::Min(VelocityZ + CurrentWallImpulse, MaxAdditiveDodgeJumpSpeed);
		//UE_LOG(UTNet, Warning, TEXT("Wall dodge at %f velZ %f"), CharacterOwner->GetWorld()->GetTimeSeconds(), Velocity.Z);
	}
	else
	{
		Velocity.Z = VelocityZ;
	}
	bExplicitJump = true;
	bIsDodging = true;
	bNotifyApex = true;
	if (IsMovingOnGround())
	{
		SetMovementMode(MOVE_Falling);
	}
	return true;
}

void UUTCharacterMovement::HandleCrouchRequest()
{
	// if moving fast enough and pressing on move key, slide, else crouch
	AUTCharacter* UTCharacterOwner = Cast<AUTCharacter>(CharacterOwner);
	UpdateFloorSlide(true);
	if (!Acceleration.IsNearlyZero() && (Velocity.Size() > MaxWalkSpeedCrouched) && UTCharacterOwner && UTCharacterOwner->CanDodge())
	{
		bPressedSlide = true;
		if (IsMovingOnGround())
		{
			PerformFloorSlide(Velocity.GetSafeNormal());
		}
		NeedsClientAdjustment();
	}
	else
	{
		bWantsToCrouch = true;
	}
}

void UUTCharacterMovement::HandleUnCrouchRequest()
{
	bWantsToCrouch = false;
	UpdateFloorSlide(false);
}

void UUTCharacterMovement::Crouch(bool bClientSimulation)
{
	float RealCrouchHeight = CrouchedHalfHeight;
	CrouchedHalfHeight = bIsFloorSliding ? FloorSlideHalfHeight : CrouchedHalfHeight;
	Super::Crouch(bClientSimulation);
	CrouchedHalfHeight = RealCrouchHeight;
}

bool UUTCharacterMovement::IsCrouching() const
{
	return CharacterOwner && CharacterOwner->bIsCrouched && !bIsFloorSliding;
}

void UUTCharacterMovement::PerformMovement(float DeltaSeconds)
{
	if (!CharacterOwner)
	{
		return;
	}
	AUTCharacter* UTOwner = Cast<AUTCharacter>(CharacterOwner);

	if (!UTOwner || !UTOwner->IsRagdoll())
	{
		float RealGroundFriction = GroundFriction;
		if (bIsFloorSliding)
		{
			GroundFriction = 0.f;
		}
		else if (bWasFloorSlideing && (MovementMode != MOVE_Falling))
		{
			Velocity *= FloorSlideEndingSpeedFactor;
		}
		bWasFloorSlideing = bIsFloorSliding;

		bool bSavedWantsToCrouch = bWantsToCrouch;
		bWantsToCrouch = bWantsToCrouch || bIsFloorSliding;
		bForceMaxAccel = bIsFloorSliding;
		FVector Loc = CharacterOwner->GetActorLocation();
		/*
		if (CharacterOwner->Role < ROLE_Authority)
		{
		UE_LOG(UTNet, Warning, TEXT("CLIENT MOVE at %f deltatime %f from %f %f %f vel %f %f %f accel %f %f %f wants to crouch %d sliding %d sprinting %d pressed slide %d"), GetCurrentSynchTime(), DeltaSeconds, Loc.X, Loc.Y, Loc.Z, Velocity.X, Velocity.Y, Velocity.Z, Acceleration.X, Acceleration.Y, Acceleration.Z, bWantsToCrouch, bIsFloorSliding, bIsSprinting, bPressedSlide);
		}
		else
		{
		UE_LOG(UTNet, Warning, TEXT("SERVER Move at %f deltatime %f from %f %f %f vel %f %f %f accel %f %f %f wants to crouch %d sliding %d sprinting %d pressed slide %d"), GetCurrentSynchTime(), DeltaSeconds, Loc.X, Loc.Y, Loc.Z, Velocity.X, Velocity.Y, Velocity.Z, Acceleration.X, Acceleration.Y, Acceleration.Z, bWantsToCrouch, bIsFloorSliding, bIsSprinting, bPressedSlide);
		}
		*/
		Super::PerformMovement(DeltaSeconds);
		bWantsToCrouch = bSavedWantsToCrouch;
		GroundFriction = RealGroundFriction;
	}

	if (UTOwner != NULL)
	{
		UTOwner->PositionUpdated(bShotSpawned);
		bShotSpawned = false;
		// tick movement reduction timer
		// we do this based on the client's movement timestamp to minimize corrections
		UTOwner->WalkMovementReductionTime -= DeltaSeconds;
		if (UTOwner->WalkMovementReductionTime <= 0.0f)
		{
			UTOwner->WalkMovementReductionPct = 0.0f;
		}
	}
}

float UUTCharacterMovement::GetMaxAcceleration() const
{
	float Result;
	if (bIsFloorSliding)
	{
		Result = FloorSlideAcceleration;
	}
	else if (MovementMode == MOVE_Falling)
	{
		Result = MaxFallingAcceleration;
	}
	else
	{
		Result = Super::GetMaxAcceleration();
		if (bIsSprinting && Velocity.SizeSquared() > FMath::Square<float>(MaxWalkSpeed))
		{
			// smooth transition to sprinting accel to avoid client/server synch issues
			const float CurrentSpeed = Velocity.Size();
			const float Transition = FMath::Min(1.f, 0.1f*(CurrentSpeed - MaxWalkSpeed));
			Result = SprintAccel*Transition + Result*(1.f - Transition);
		}
		Result = (bIsSprinting && Velocity.SizeSquared() > FMath::Square<float>(MaxWalkSpeed)) ? SprintAccel : Super::GetMaxAcceleration();
	}
	if (MovementMode == MOVE_Walking && Cast<AUTCharacter>(CharacterOwner) != NULL)
	{
		Result *= (1.0f - ((AUTCharacter*)CharacterOwner)->GetWalkMovementReductionPct());
	}
	return Result;
}

bool UUTCharacterMovement::CanSprint() const
{
	if (CharacterOwner && IsMovingOnGround() && !IsCrouching() && (GetCurrentMovementTime() > SprintStartTime)) 
	{
		// must be moving mostly forward
		FRotator TurnRot(0.f, CharacterOwner->GetActorRotation().Yaw, 0.f);
		FVector X = FRotationMatrix(TurnRot).GetScaledAxis(EAxis::X);
		return ((X | Velocity.GetSafeNormal()) > 0.6f);
	}
	return false;
}

float UUTCharacterMovement::GetMaxSpeed() const
{
	// ignore standard movement while character is a ragdoll
	if (Cast<AUTCharacter>(CharacterOwner) != NULL && ((AUTCharacter*)CharacterOwner)->IsRagdoll())
	{
		// small non-zero number used to avoid divide by zero issues
		return 0.01f;
	}
	else if (bIsEmoting)
	{
		return 0.01f;
	}
	else if (bIsFloorSliding && (MovementMode == MOVE_Walking))
	{
		// higher max velocity if going down hill
		float CurrentSpeed = Velocity.Size();
		if ((CurrentSpeed > MaxFloorSlideSpeed) && (CurrentFloor.HitResult.ImpactNormal.Z < 1.f))
		{
			float TopSlideSpeed = FMath::Min(SprintSpeed, CurrentSpeed);
			return FMath::Min(TopSlideSpeed, MaxFloorSlideSpeed + FMath::Max(0.f, (Velocity | CurrentFloor.HitResult.ImpactNormal)));
		}
		return MaxFloorSlideSpeed;
	}
	else if (bFallingInWater && (MovementMode == MOVE_Falling))
	{
		return MaxWaterSpeed;
	}
	else if (MovementMode == MOVE_Swimming)
	{
		AUTWaterVolume* WaterVolume = Cast<AUTWaterVolume>(GetPhysicsVolume());
		return WaterVolume ? FMath::Min(MaxSwimSpeed, WaterVolume->MaxRelativeSwimSpeed) : MaxSwimSpeed;
	}
	else
	{
		return bIsSprinting ? SprintSpeed : Super::GetMaxSpeed();
	}
}

void UUTCharacterMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (Acceleration.IsZero())
	{
		SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

void UUTCharacterMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (MovementMode == MOVE_Walking)
	{
		AUTCharacter* UTOwner = Cast<AUTCharacter>(CharacterOwner);
		if (UTOwner != NULL)
		{
			Friction *= (1.0f - UTOwner->GetWalkMovementReductionPct());
			BrakingDeceleration *= (1.0f - UTOwner->GetWalkMovementReductionPct());
		}
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
	//UE_LOG(UTNet, Warning, TEXT("At %f DeltaTime %f Velocity is %f %f %f from acceleration %f %f slide %d"), GetCurrentSynchTime(), DeltaTime, Velocity.X, Velocity.Y, Velocity.Z, Acceleration.X, Acceleration.Y, bIsFloorSliding);

	// workaround for engine path following code not setting Acceleration correctly
	if (bHasRequestedVelocity && Acceleration.IsZero())
	{
		Acceleration = Velocity.GetSafeNormal();
	}
}

void UUTCharacterMovement::ResetTimers()
{
	DodgeResetTime = 0.f;
	SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
	FloorSlideTapTime = 0.f;
	FloorSlideEndTime = 0.f;
}

float UUTCharacterMovement::FallingDamageReduction(float FallingDamage, const FHitResult& Hit)
{
	if (Hit.ImpactNormal.Z < GetWalkableFloorZ())
	{
		// Scale damage based on angle of wall we hit
		return FallingDamage * Hit.ImpactNormal.Z;
	}
	return (GetCurrentMovementTime() - FloorSlideTapTime < FloorSlideBonusTapInterval) ? FallingDamageRollReduction : 0.f;
}

void UUTCharacterMovement::RestrictJump(float RestrictedJumpTime)
{
	bRestrictedJump = true;
	GetWorld()->GetTimerManager().SetTimer(ClearRestrictedJumpHandle, this, &UUTCharacterMovement::ClearRestrictedJump, RestrictedJumpTime, false);
}

void UUTCharacterMovement::ClearRestrictedJump()
{
	bRestrictedJump = false;
	GetWorld()->GetTimerManager().ClearTimer(ClearRestrictedJumpHandle);
}

void UUTCharacterMovement::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	bIsAgainstWall = false;
	bFallingInWater = false;

	if (CharacterOwner)
	{
		bIsFloorSliding = bWantsFloorSlide;
		if (CharacterOwner->ShouldNotifyLanded(Hit))
		{
			CharacterOwner->Landed(Hit);
		}
		if (bIsFloorSliding)
		{
			PerformFloorSlide(Velocity.GetSafeNormal2D());
			//UE_LOG(UTNet, Warning, TEXT("FloorSlide within %f"), GetCurrentMovementTime() - FloorSlideTapTime);
			// @TODO FIXMESTEVE - should also update DodgeRestTime if roll but not out of dodge?
			if (bIsDodging)
			{
				DodgeResetTime = FloorSlideEndTime + ((CurrentMultiJumpCount > 0) ? DodgeJumpResetInterval : DodgeResetInterval);
				//UE_LOG(UTNet, Warning, TEXT("Set dodge reset after landing move time %f dodge reset time %f"), GetCurrentMovementTime(), DodgeResetTime);
			}
		}
		else if (bIsDodging)
		{
			Velocity *= ((CurrentMultiJumpCount > 0) ? DodgeJumpLandingSpeedFactor : DodgeLandingSpeedFactor);
			DodgeResetTime = GetCurrentMovementTime() + ((CurrentMultiJumpCount > 0) ? DodgeJumpResetInterval : DodgeResetInterval);
			//UE_LOG(UTNet, Warning, TEXT("bIsDodging cut velocity to %f %f %f"),Velocity.X, Velocity.Y, Velocity.Z);
		}
		bIsDodging = false;
		SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
	}
	bJumpAssisted = false;
	AUTCharacter* UTCharOwner = Cast<AUTCharacter>(CharacterOwner);
	if (UTCharOwner)
	{
		UTCharOwner->bApplyWallSlide = false;
	}
	bExplicitJump = false;
	ClearRestrictedJump();
	CurrentMultiJumpCount = 0;
	CurrentWallDodgeCount = 0;
	if (IsFalling())
	{
		SetPostLandedPhysics(Hit);
	}

	StartNewPhysics(remainingTime, Iterations);
}

void UUTCharacterMovement::UpdateFloorSlide(bool bNewWantsFloorSlide)
{
	if (bNewWantsFloorSlide && !bWantsFloorSlide)
	{
		FloorSlideTapTime = GetCurrentMovementTime();
	}
	bWantsFloorSlide = bNewWantsFloorSlide;
}

bool UUTCharacterMovement::WantsFloorSlide()
{ 
	return bWantsFloorSlide; 
}

void UUTCharacterMovement::PerformFloorSlide(const FVector& DodgeDir)
{
	if (CharacterOwner)
	{
		FloorSlideTapTime = GetCurrentMovementTime();
		bIsFloorSliding = true;
		FloorSlideEndTime = GetCurrentMovementTime() + FloorSlideDuration;
		Acceleration = FloorSlideAcceleration * DodgeDir;
		DodgeResetTime = FloorSlideEndTime + DodgeResetInterval;
		float NewSpeed = FMath::Max(MaxFloorSlideSpeed, FMath::Min(Velocity.Size2D(), SprintSpeed));
		Velocity = NewSpeed*DodgeDir;
		AUTCharacter* UTChar = Cast<AUTCharacter>(CharacterOwner);
		if (UTChar)
		{
			UTChar->MovementEventUpdated(EME_Slide, DodgeDir);
		}
	}
}

bool UUTCharacterMovement::DoJump(bool bReplayingMoves)
{
	if (CharacterOwner && CharacterOwner->CanJump() && (IsFalling() ? DoMultiJump() : Super::DoJump(bReplayingMoves)))
	{
		if (Cast<AUTCharacter>(CharacterOwner) != NULL)
		{
			((AUTCharacter*)CharacterOwner)->MovementEventUpdated(EME_Jump, Velocity.GetSafeNormal());
		}
		bNotifyApex = true;
		bExplicitJump = true;
		NeedsClientAdjustment(); 
		return true;
	}
	return false;
}

bool UUTCharacterMovement::DoMultiJump()
{
	if (CharacterOwner)
	{
		Velocity.Z = bIsDodging ? DodgeJumpImpulse : MultiJumpImpulse;
		CurrentMultiJumpCount++;
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
	return ( (MaxMultiJumpCount > 0) && (CurrentMultiJumpCount < MaxMultiJumpCount) && (!bIsDodging || bAllowDodgeMultijumps) && (bIsDodging || bAllowJumpMultijumps) &&
			(bAlwaysAllowFallingMultiJump ? (Velocity.Z < MaxMultiJumpZSpeed) : (FMath::Abs(Velocity.Z) < MaxMultiJumpZSpeed)) );
}

void UUTCharacterMovement::ClearDodgeInput()
{
	//UE_LOG(UTNet, Warning, TEXT("ClearDodgeInput"));
	bPressedDodgeForward = false;
	bPressedDodgeBack = false;
	bPressedDodgeLeft = false;
	bPressedDodgeRight = false;
	bPressedSlide = false;
}

void UUTCharacterMovement::CheckJumpInput(float DeltaTime)
{
	if (CharacterOwner && CharacterOwner->bPressedJump)
	{
		if ((MovementMode == MOVE_Walking) || (MovementMode == MOVE_Falling))
		{
			DoJump(CharacterOwner->bClientUpdating);
		}
		else if ((MovementMode == MOVE_Swimming) && CanDodge())
		{
			PerformWaterJump();
		}
	}
	else if (bPressedDodgeForward || bPressedDodgeBack || bPressedDodgeLeft || bPressedDodgeRight)
	{
		float DodgeDirX = bPressedDodgeForward ? 1.f : (bPressedDodgeBack ? -1.f : 0.f);
		float DodgeDirY = bPressedDodgeLeft ? -1.f : (bPressedDodgeRight ? 1.f : 0.f);
		float DodgeCrossX = (bPressedDodgeLeft || bPressedDodgeRight) ? 1.f : 0.f;
		float DodgeCrossY = (bPressedDodgeForward || bPressedDodgeBack) ? 1.f : 0.f;
		FRotator TurnRot(0.f, CharacterOwner->GetActorRotation().Yaw, 0.f);
		FRotationMatrix TurnRotMatrix = FRotationMatrix(TurnRot);
		FVector X = TurnRotMatrix.GetScaledAxis(EAxis::X);
		FVector Y = TurnRotMatrix.GetScaledAxis(EAxis::Y);
		AUTCharacter* UTCharacterOwner = Cast<AUTCharacter>(CharacterOwner);
		if (UTCharacterOwner)
		{
			UTCharacterOwner->Dodge((DodgeDirX*X + DodgeDirY*Y).GetSafeNormal(), (DodgeCrossX*X + DodgeCrossY*Y).GetSafeNormal());
		}
	}

	if (CharacterOwner)
	{
		// If server, we already got these flags from the saved move
		if (CharacterOwner->IsLocallyControlled())
		{
			bIsFloorSliding = bIsFloorSliding && (GetCurrentMovementTime() < FloorSlideEndTime);
			//bool bWasSprinting = bIsSprinting;
			bIsSprinting = CanSprint();
			/*if (bWasSprinting != bIsSprinting)
			{
				UE_LOG(UTNet, Warning, TEXT("SPRINTING NOW %d"), bIsSprinting);
			}*/
		}

		if (!bIsFloorSliding && bWasFloorSlideing)
		{
			SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
			AUTCharacter* UTCharacterOwner = Cast<AUTCharacter>(CharacterOwner);
			if (UTCharacterOwner)
			{
				UTCharacterOwner->DesiredJumpBob = FVector(0.f);
			}
		}
	}
}

bool UUTCharacterMovement::Is3DMovementMode() const
{
	return (MovementMode == MOVE_Flying) || (MovementMode == MOVE_Swimming);
}

/** @TODO FIXMESTEVE - update super class with this? */
FVector UUTCharacterMovement::ComputeSlideVectorUT(const float DeltaTime, const FVector& InDelta, const float Time, const FVector& Normal, const FHitResult& Hit)
{
	const bool bFalling = IsFalling();
	FVector Delta = InDelta;
	FVector Result = UMovementComponent::ComputeSlideVector(Delta, Time, Normal, Hit);
	
	// prevent boosting up slopes
	if (bFalling && Result.Z > 0.f)
	{
		// @TODO FIXMESTEVE - make this a virtual function in super class so just change this part
		float PawnRadius, PawnHalfHeight;
		CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
		if (Delta.Z < 0.f && (Hit.ImpactNormal.Z < MAX_STEP_SIDE_Z))
		{
			// We were moving downward, but a slide was going to send us upward. We want to aim
			// straight down for the next move to make sure we get the most upward-facing opposing normal.
			Result = FVector(0.f, 0.f, Delta.Z);
		}
		else if (bAllowSlopeDodgeBoost && (((CharacterOwner->GetActorLocation() - Hit.ImpactPoint).Size2D() > 0.93f * PawnRadius) || (Hit.ImpactNormal.Z > 0.2f))) // @TODO FIXMESTEVE tweak magic numbers
		{
			if (Result.Z > Delta.Z*Time)
			{
				Result.Z = FMath::Max(Result.Z * SlopeDodgeScaling, Delta.Z*Time);
			}
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
				const FVector NormalXY = Normal.GetSafeNormal2D();
				const FVector Adjust = Super::ComputeSlideVector(RemainderXY, 1.f, NormalXY, Hit);
				Result += Adjust;
			}
		}
	}
	return Result;
}

bool UUTCharacterMovement::CanCrouchInCurrentState() const
{
	// @TODO FIXMESTEVE Temp hack until we can get the crouch control code split out from PerformMovement()
	if (IsCrouching() && !bIsFloorSliding && (CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() < CrouchedHalfHeight))
	{
		return false;
	}
	return CanEverCrouch() && IsMovingOnGround();
}

void UUTCharacterMovement::CheckWallSlide(FHitResult const& Impact)
{
	AUTCharacter* UTCharOwner = Cast<AUTCharacter>(CharacterOwner);
	if (UTCharOwner)
	{
		UTCharOwner->bApplyWallSlide = false;
		// @TODO FIXMESTEVE bWantsFloorSlide only for floor slide now!
		if ((bWantsFloorSlide || bAutoSlide) && (Velocity.Z < 0.f) && bExplicitJump && (Velocity.Z > MaxSlideFallZ) && !Acceleration.IsZero() && ((Acceleration.GetSafeNormal() | Impact.ImpactNormal) < MaxSlideAccelNormal))
		{
			FVector VelocityAlongWall = Velocity + (Velocity | Impact.ImpactNormal);
			UTCharOwner->bApplyWallSlide = (VelocityAlongWall.Size2D() >= MinWallSlideSpeed);
			if (UTCharOwner->bApplyWallSlide && UTCharOwner->bCanPlayWallHitSound)
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), UTCharOwner->WallHitSound, CharacterOwner, SRT_None);
				UTCharOwner->bCanPlayWallHitSound = false;
			}
		}
	}
}

void UUTCharacterMovement::HandleImpact(FHitResult const& Impact, float TimeSlice, const FVector& MoveDelta)
{
	if (IsFalling())
	{
		CheckWallSlide(Impact);
	}
	AActor* ImpactActor = Impact.GetActor();
	if (ImpactActor && ImpactActor->GetRootComponent() && (ImpactActor->GetRootComponent()->Mobility == EComponentMobility::Movable))
	{
		NeedsClientAdjustment();
	}
	float ImpactDot = Impact.ImpactNormal | Velocity.GetSafeNormal();
	if (bIsSprinting && (ImpactDot < SprintMaxWallNormal))
	{
		SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
	}
	Super::HandleImpact(Impact, TimeSlice, MoveDelta);
}

bool UUTCharacterMovement::CanBaseOnLift(UPrimitiveComponent* LiftPrim, const FVector& LiftMoveDelta)
{
	// If character jumped off this lift and is still going up fast enough, then just push him along
	if (Velocity.Z > 0.f)
	{
		FVector LiftVel = MovementBaseUtility::GetMovementBaseVelocity(LiftPrim, NAME_None);
		if (LiftVel.Z > 0.f)
		{
			FHitResult Hit(1.f);
			FVector MoveDelta(0.f);
			MoveDelta.Z = LiftMoveDelta.Z;
			SafeMoveUpdatedComponent(MoveDelta, CharacterOwner->GetActorRotation(), true, Hit);
			return true;
		}
	}
	const FVector PawnLocation = CharacterOwner->GetActorLocation();
	FFindFloorResult FloorResult;
	FindFloor(PawnLocation, FloorResult, false);
	if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
	{
		if (IsFalling())
		{
			ProcessLanded(FloorResult.HitResult, 0.f, 1);
		}
		else if (IsMovingOnGround())
		{
			AdjustFloorHeight();
			SetBase(FloorResult.HitResult.Component.Get(), FloorResult.HitResult.BoneName);
		}
		else
		{
			return false;
		}
		return (CharacterOwner->GetMovementBase() == LiftPrim);
	}
	return false;
}

float UUTCharacterMovement::GetGravityZ() const
{
	AUTCharacter* UTCharOwner = Cast<AUTCharacter>(CharacterOwner);
	if (UTCharOwner)
	{
		return Super::GetGravityZ() * (UTCharOwner->bApplyWallSlide ? SlideGravityScaling : 1.f);
	}
	return Super::GetGravityZ();
}

void UUTCharacterMovement::PhysSwimming(float deltaTime, int32 Iterations)
{
	if (Velocity.Size() > MaxWaterSpeed)
	{
		FVector VelDir = Velocity.GetSafeNormal();
		if ((VelDir | Acceleration) > 0.f)
		{
			Acceleration = Acceleration - (VelDir | Acceleration)*VelDir; 
		}
	}

	ApplyWaterCurrent(deltaTime);
	if (!GetPhysicsVolume()->bWaterVolume && IsSwimming())
	{
		SetMovementMode(MOVE_Falling); //in case script didn't change it (w/ zone change)
	}

	//may have left water - if so, script might have set new physics mode
	if (!IsSwimming())
	{
		// include water current velocity if leave water (different frame of reference)
		StartNewPhysics(deltaTime, Iterations);
		return;
	}
	Super::PhysSwimming(deltaTime, Iterations);
}

float UUTCharacterMovement::ComputeAnalogInputModifier() const
{
	return 1.f;
}

bool UUTCharacterMovement::ShouldJumpOutOfWater(FVector& JumpDir)
{
	// only consider velocity, not view dir
	if (Velocity.Z > 0.f)
	{
		JumpDir = Acceleration.GetSafeNormal();
		return true;
	}
	return false;
}

void UUTCharacterMovement::ApplyWaterCurrent(float DeltaTime)
{
	if (!CharacterOwner)
	{
		return;
	}
	AUTWaterVolume* WaterVolume = Cast<AUTWaterVolume>(GetPhysicsVolume());
	if (!WaterVolume && bFallingInWater)
	{
		FVector FootLocation = GetActorLocation() - FVector(0.f, 0.f, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		// check if touching water with current
		TArray<FOverlapResult> Hits;
		static FName NAME_PhysicsVolumeTrace = FName(TEXT("PhysicsVolumeTrace"));
		FComponentQueryParams Params(NAME_PhysicsVolumeTrace, CharacterOwner->GetOwner());
		GetWorld()->OverlapMultiByChannel(Hits, FootLocation, FQuat::Identity, CharacterOwner->GetCapsuleComponent()->GetCollisionObjectType(), FCollisionShape::MakeSphere(0.f), Params);

		for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
		{
			const FOverlapResult& Link = Hits[HitIdx];
			AUTWaterVolume* const V = Cast<AUTWaterVolume>(Link.GetActor());
			if (V && (!WaterVolume || (V->Priority > WaterVolume->Priority)))
			{
				WaterVolume = V;
			}
		}
	}
	bFallingInWater = false;
	if (WaterVolume)
	{
		bFallingInWater = true;
		// apply any water current
		// current force is not added to velocity (velocity is relative to current, not limited by it)
		FVector WaterCurrent = WaterVolume ? WaterVolume->GetCurrentFor(CharacterOwner) : FVector(0.f);
		if (!WaterCurrent.IsNearlyZero())
		{
	
			// current force is not added to velocity (velocity is relative to current, not limited by it)
			FVector Adjusted = WaterCurrent*DeltaTime;
			FHitResult Hit(1.f);
			SafeMoveUpdatedComponent(Adjusted, CharacterOwner->GetActorRotation(), true, Hit);
			float remainingTime = DeltaTime * (1.f - Hit.Time);

			if (Hit.Time < 1.f && CharacterOwner)
			{
				//adjust and try again
				HandleImpact(Hit, remainingTime, Adjusted);
				SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
			}
		}
	}
}

/** @TODO FIXMESTEVE - physfalling copied from base version and edited.  At some point should probably add some hooks to base version and use those instead. */
void UUTCharacterMovement::PhysFalling(float deltaTime, int32 Iterations)
{
	ApplyWaterCurrent(deltaTime);
	if (!IsFalling())
	{
		// include water current velocity if leave water (different frame of reference)
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	// Bound final 2d portion of velocity
	const float Speed2d = Velocity.Size2D();
	const float BoundSpeed = FMath::Max(Speed2d, GetMaxSpeed() * AnalogInputModifier);

	//bound acceleration, falling object has minimal ability to impact acceleration
	FVector FallAcceleration = Acceleration;
	FallAcceleration.Z = 0.f;

	if ((CurrentWallDodgeCount > 0) && (Velocity.Z > 0.f) && ((FallAcceleration | LastWallDodgeNormal) < 0.f) && ((FallAcceleration.GetSafeNormal() | LastWallDodgeNormal) < -1.f*WallDodgeMinNormal))
	{
		// don't air control back into wall you just dodged from  
		FallAcceleration = FallAcceleration - (FallAcceleration | LastWallDodgeNormal) * LastWallDodgeNormal;
		FallAcceleration = FallAcceleration.GetSafeNormal();
	}
	bool bSkipLandingAssist = true;
	AUTCharacter* UTCharOwner = Cast<AUTCharacter>(CharacterOwner);
	if (UTCharOwner)
	{
		UTCharOwner->bApplyWallSlide = false;
	}
	FHitResult Hit(1.f);
	bIsAgainstWall = false;
	if (!HasRootMotion())
	{
		// test for slope to avoid using air control to climb walls 
		float TickAirControl = (CurrentMultiJumpCount < 1) ? AirControl : MultiJumpAirControl;
		if (bRestrictedJump)
		{
			TickAirControl = 0.0f;
		}
		if (TickAirControl > 0.0f && FallAcceleration.SizeSquared() > 0.f)
		{
			const float TestWalkTime = FMath::Max(deltaTime, 0.05f);
			const FVector TestWalk = ((TickAirControl * GetMaxAcceleration() * FallAcceleration.GetSafeNormal() + FVector(0.f, 0.f, GetGravityZ())) * TestWalkTime + Velocity) * TestWalkTime;
			if (!TestWalk.IsZero())
			{
				static const FName FallingTraceParamsTag = FName(TEXT("PhysFalling"));
				FHitResult Result(1.f);
				FCollisionQueryParams CapsuleQuery(FallingTraceParamsTag, false, CharacterOwner);
				FCollisionResponseParams ResponseParam;
				InitCollisionParams(CapsuleQuery, ResponseParam);
				const FVector PawnLocation = CharacterOwner->GetActorLocation();
				const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
				const bool bHit = GetWorld()->SweepSingleByChannel(Result, PawnLocation, PawnLocation + TestWalk, FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);
				if (bHit)
				{
					bSkipLandingAssist = false;
					// Only matters if we can't walk there
					if (!IsValidLandingSpot(Result.Location, Result))
					{
						// We are against the wall, store info about it
						bIsAgainstWall = true;
						WallSlideNormal = Result.Normal;
						TickAirControl = 0.f;
						CheckWallSlide(Result);
					}
				}
			}
		}

		// Boost maxAccel to increase player's control when falling straight down
		if ((Speed2d < 25.f) && (TickAirControl > 0.f)) //allow initial burst
		{
			TickAirControl = FMath::Min(1.f, 2.f*TickAirControl);
		}

		float MaxAccel = GetMaxAcceleration() * TickAirControl;				

		FallAcceleration = FallAcceleration.GetClampedToMaxSize(MaxAccel);
	}

	float remainingTime = deltaTime;
	float timeTick = 0.1f;
	/*
	FVector Loc = CharacterOwner->GetActorLocation();
	if (CharacterOwner->Role < ROLE_Authority)
	{
		UE_LOG(UTNet, Warning, TEXT("CLIENT Fall at %f from %f %f %f vel %f %f %f delta %f"), GetCurrentSynchTime(), Loc.X, Loc.Y, Loc.Z, Velocity.X, Velocity.Y, Velocity.Z, deltaTime);
	}
	else
	{
		UE_LOG(UTNet, Warning, TEXT("SERVER Fall at %f from %f %f %f vel %f %f %f delta %f"), GetCurrentSynchTime(), Loc.X, Loc.Y, Loc.Z, Velocity.X, Velocity.Y, Velocity.Z, deltaTime);
	}
	*/
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
			Velocity = Velocity.GetClampedToMaxSize2D(BoundSpeed);
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
			if (UTCharOwner)
			{
				UTCharOwner->bApplyWallSlide = false;
			}
			StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
			return;
		}
		else if (Hit.Time < 1.f)
		{
			//UE_LOG(UT, Warning, TEXT("HIT WALL %f"), Hit.Time);
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
					if (UTCharOwner)
					{
						UTCharOwner->bApplyWallSlide = false;
					}
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
							if (UTCharOwner)
							{
								UTCharOwner->bApplyWallSlide = false;
							}
							return;
						}

						TwoWallAdjust(Delta, Hit, OldHitNormal);

						// bDitch=true means that pawn is straddling two slopes, neither of which he can stand on
						bool bDitch = ((OldHitImpactNormal.Z > 0.f) && (Hit.ImpactNormal.Z > 0.f) && (FMath::Abs(Delta.Z) <= KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));
						SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
						if (Hit.Time == 0.f)
						{
							// if we are stuck then try to side step
							FVector SideDelta = (OldHitNormal + Hit.ImpactNormal).GetSafeNormal2D();
							if (SideDelta.IsNearlyZero())
							{
								SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0).GetSafeNormal();
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
		if (UTCharOwner)
		{
			UTCharOwner->bApplyWallSlide = false;
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

			Velocity = Velocity.GetClampedToMaxSize(GetPhysicsVolume()->TerminalVelocity);
		}
		//UE_LOG(UT, Warning, TEXT("FINAL VELOCITY at %f vel %f %f %f"), GetCurrentSynchTime(), Velocity.X, Velocity.Y, Velocity.Z);
	}
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

	// See if stepping up/forward in acceleration direction would result in valid landing
	FHitResult Result(1.f);
	static const FName LandAssistTraceParamsTag = FName(TEXT("LandAssist"));
	FCollisionQueryParams CapsuleQuery(LandAssistTraceParamsTag, false, CharacterOwner);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(CapsuleQuery, ResponseParam);
	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
	bool bHit = GetWorld()->SweepSingleByChannel(Result, PawnLocation, PawnLocation + FVector(0.f, 0.f, LandingStepUp), FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);
	FVector HorizontalStart = bHit ? Result.Location : PawnLocation + FVector(0.f, 0.f, LandingStepUp);
	float ElapsedTime = 0.05f; // FMath::Min(0.05f, remainingTime);
	FVector HorizontalDir = Acceleration.GetSafeNormal2D() * MaxWalkSpeed * 0.05f;;
	bHit = GetWorld()->SweepSingleByChannel(Result, HorizontalStart, HorizontalStart + HorizontalDir, FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);
	FVector LandingStart = bHit ? Result.Location : HorizontalStart + HorizontalDir;
	bHit = GetWorld()->SweepSingleByChannel(Result, LandingStart, LandingStart - FVector(0.f, 0.f, LandingStepUp), FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);

	if (IsValidLandingSpot(Result.Location, Result))
	{
		// Found a valid landing spot, so boost the player up onto it.
		bJustTeleported = true;
		if (Cast<AUTCharacter>(CharacterOwner))
		{
			Cast<AUTCharacter>(CharacterOwner)->OnLandingAssist();
		}
		Velocity.Z = LandingAssistBoost; 
	}
}


