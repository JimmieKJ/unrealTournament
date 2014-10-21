// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacterMovement.h"
#include "GameFramework/GameNetworkManager.h"
#include "UTLift.h"
#include "UTReachSpec_Lift.h"
#include "UTBot.h"

const float MAX_STEP_SIDE_Z = 0.08f;	// maximum z value for the normal on the vertical side of steps

UUTCharacterMovement::UUTCharacterMovement(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	// @TODO FIXMESTEVE increase this to hour+ for release version
	// MinTimeBetweenTimeStampResets = 3600.f;

	MaxWalkSpeed = 950.f;
	MaxCustomMovementSpeed = MaxWalkSpeed;

	WallDodgeTraceDist = 50.f;
	MinAdditiveDodgeFallSpeed = -5000.f;  
	MaxAdditiveDodgeJumpSpeed = 700.f;  
	MaxMultiJumpCount = 1;
	bAllowDodgeMultijumps = false;
	bAllowJumpMultijumps = true;
	bCountWallDodgeMultijumps = true;
	MultiJumpImpulse = 600.f;
	DodgeJumpImpulse = 600.f;
	DodgeLandingSpeedFactor = 0.19f;
	DodgeJumpLandingSpeedFactor = 0.19f;
	DodgeResetInterval = 0.35f;
	DodgeJumpResetInterval = 0.35f;
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
	MaxConsecutiveWallDodgeDP = 0.97f;
	WallDodgeGraceVelocityZ = -2400.f;
	AirControl = 0.4f;
	MultiJumpAirControl = 0.4f;
	bAllowSlopeDodgeBoost = true;
	SetWalkableFloorZ(0.695f); 
	MaxAcceleration = 4200.f; 
	BrakingDecelerationWalking = 4800.f;
	BrakingDecelerationSwimming = 300.f;
	GravityScale = 1.f;
	DodgeImpulseHorizontal = 1350.f;
	DodgeMaxHorizontalVelocity = 1500.f; // DodgeImpulseHorizontal * 1.11
	MaxStepHeight = 51.0f;
	NavAgentProps.AgentStepHeight = MaxStepHeight; // warning: must be manually mirrored, won't be set automatically
	CrouchedHalfHeight = 64.0f;
	RollHalfHeight = 46.f;
	SlopeDodgeScaling = 0.93f;

	DodgeRollAcceleration = 2000.f;
	MaxDodgeRollSpeed = 920.f;
	DodgeRollDuration = 0.45f;
	DodgeRollBonusTapInterval = 0.17f;
	DodgeRollEarliestZ = -100.f;
	RollEndingSpeedFactor = 0.5f;
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
	bJustDodged = false;					
	bIsDodgeRolling = false;				
	DodgeRollTapTime = 0.f;					
	DodgeRollEndTime = 0.f;					
	CurrentMultiJumpCount = 0;				
	CurrentWallDodgeCount = 0;				
	bWantsSlideRoll = false;				
	bApplyWallSlide = false;			
	SavedAcceleration = FVector(0.f);		
	bMaintainSlideRollAccel = true;			
	bHasCheckedAgainstWall = false;			
	bIsSettingUpFirstReplayMove = false;

	EasyImpactImpulse = 1050.f;
	EasyImpactDamage = 25;
	FullImpactImpulse = 1500.f;
	FullImpactDamage = 40;
	ImpactMaxHorizontalVelocity = 1500.f;
	ImpactMaxVerticalFactor = 1.f;
	MaxUndampedImpulse = 2000.f;

	OutofWaterZ = 700.f;
	JumpOutOfWaterPitch = 0.f;
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
							int32 LinkIndex = B->GetMoveTarget().Node->GetBestLinkTo(B->GetMoveTarget().TargetPoly, B->RouteCache[i + 1], CharacterOwner, *CharacterOwner->GetNavAgentProperties(), GetUTNavData(GetWorld()));
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
						Velocity = (LiftPath->LiftExitLoc - PawnLoc).SafeNormal2D() * MaxWalkSpeed;
						DoJump(false);
						// redirect bot to next point on route if necessary
						if (ExitRouteIndex != INDEX_NONE)
						{
							TArray<FComponentBasedPosition> MovePoints;
							new(MovePoints) FComponentBasedPosition(LiftPath->LiftExitLoc);
							B->SetMoveTarget(B->RouteCache[ExitRouteIndex], MovePoints);
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
		NewVelocity = NewVelocity.SafeNormal2D() * ImpactMaxHorizontalVelocity;
		NewVelocity.Z = VelZ;
	}
	NewVelocity.Z = FMath::Min(NewVelocity.Z, ImpactMaxVerticalFactor*ImpulseMag);
	Velocity = NewVelocity;
	SetMovementMode(MOVE_Falling);
	bNotifyApex = true;
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

void UUTCharacterMovement::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	AUTCharacter* UTOwner = Cast<AUTCharacter>(CharacterOwner);
	if (UTOwner == NULL || !UTOwner->IsRagdoll())
	{
		UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

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
			if (bIsClient)
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
					DeltaTime = ClientData->UpdateTimeStampAndDeltaTime(DeltaTime, *CharacterOwner, *this);
					CurrentServerMoveTime = ClientData->CurrentTimeStamp;
				}
				else
				{
					CurrentServerMoveTime = GetWorld()->GetTimeSeconds();
				}
				//UE_LOG(UT, Warning, TEXT("Correction COMPLETE"));
				// We need to check the jump state before adjusting input acceleration, to minimize latency
				// and to make sure acceleration respects our potentially new falling state.
				CharacterOwner->CheckJumpInput(DeltaTime);

				// apply input to acceleration
				Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector));
				AnalogInputModifier = ComputeAnalogInputModifier();

				if (CharacterOwner->Role == ROLE_Authority)
				{
					PerformMovement(DeltaTime);
				}
				else if (bIsClient)
				{
					ReplicateMoveToServer(DeltaTime, Acceleration);
				}
			}
			else if (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy)
			{
				// Server ticking for remote client.
				// Between net updates from the client we need to update position if based on another object,
				// otherwise the object will move on intermediate frames and we won't follow it.
				MaybeUpdateBasedMovement(DeltaTime);
				SaveBaseLocation();
			}
		}
		else if (CharacterOwner->Role == ROLE_SimulatedProxy)
		{
			AdjustProxyCapsuleSize();
			SimulatedTick(DeltaTime);
		}

		if (bEnablePhysicsInteraction)
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
		FVector PendingVelocityDir = PendingVelocity.SafeNormal();
		FVector AdditiveImpulse = PendingVelocityDir * (PendingVelocityDir | FinalImpulse);
		FVector OrthogonalImpulse = FinalImpulse - AdditiveImpulse;
		FVector ResultVelocity = PendingVelocity + AdditiveImpulse;
		float CurrentXYSpeed = PendingVelocity.Size2D();
		float ResultXYSpeed = ResultVelocity.Size2D();
		float XYDelta = ResultXYSpeed - CurrentXYSpeed;
		if (XYDelta > 0.f)
		{
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
	}
}

bool UUTCharacterMovement::ClientUpdatePositionAfterServerUpdate()
{
	if (!HasValidData())
	{
		return false;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();

	if (!ClientData->bUpdatePosition)
	{
		return false;
	}

	// Save important values that might get affected by the replay.
	const bool bRealWantsSlideRoll = bWantsSlideRoll;

	// revert to old values and let replays update them
	if (ClientData->SavedMoves.Num() > 0)
	{
		bIsSettingUpFirstReplayMove = true;
		const FSavedMovePtr& FirstMove = ClientData->SavedMoves[0];
		FirstMove->PrepMoveFor(CharacterOwner);
		bIsSettingUpFirstReplayMove = false;
	}
	bool bResult = Super::ClientUpdatePositionAfterServerUpdate();

	// Restore saved values.
	bWantsSlideRoll = bRealWantsSlideRoll;

	return bResult;
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
			Result = (Velocity + Result).SafeNormal() * FMath::Sqrt(MaxSpeedSq) - Velocity;
		}
		Result.Z = FMath::Max(Result.Z, 0.f);
	}

	return Result;
}

bool UUTCharacterMovement::CanDodge()
{
	//if (GetCurrentMovementTime() < DodgeResetTime)
	{
		//UE_LOG(UT, Warning, TEXT("Failed dodge current move time %f dodge reset time %f"), GetCurrentMovementTime(), DodgeResetTime);
	}
	return !bIsDodgeRolling && CanEverJump() && (GetCurrentMovementTime() > DodgeResetTime);
}

bool UUTCharacterMovement::CanJump()
{
	return (IsMovingOnGround() || CanMultiJump()) && CanEverJump() && !bWantsToCrouch && !bIsDodgeRolling;
}

void UUTCharacterMovement::PerformWaterJump()
{
	if (!HasValidData())
	{
		return;
	}

	float PawnCapsuleRadius, PawnCapsuleHalfHeight;
	CharacterOwner->CapsuleComponent->GetScaledCapsuleSize(PawnCapsuleRadius, PawnCapsuleHalfHeight);
	FVector TraceStart = CharacterOwner->GetActorLocation();
	TraceStart.Z = TraceStart.Z - PawnCapsuleHalfHeight + PawnCapsuleRadius;
	FVector TraceEnd = TraceStart - FVector(0.f, 0.f, WallDodgeTraceDist);

	static const FName DodgeTag = FName(TEXT("Dodge"));
	FCollisionQueryParams QueryParams(DodgeTag, false, CharacterOwner);
	FHitResult Result;
	const bool bBlockingHit = GetWorld()->SweepSingle(Result, TraceStart, TraceEnd, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(), FCollisionShape::MakeSphere(PawnCapsuleRadius), QueryParams);
	if (!bBlockingHit)
	{
		return;
	}
	DodgeResetTime = GetCurrentMovementTime() + WallDodgeResetInterval;
	if (!CharacterOwner->bClientUpdating)
	{
		//UE_LOG(UT, Warning, TEXT("Set dodge reset after wall dodge move time %f dodge reset time %f"), GetCurrentMovementTime(), DodgeResetTime);

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
	UE_LOG(UT, Warning, TEXT("Perform dodge at %f loc %f %f %f vel %f %f %f dodgedir %f %f %f from yaw %f"), GetCurrentSynchTime(), Loc.X, Loc.Y, Loc.Z, Velocity.X, Velocity.Y, Velocity.Z, DodgeDir.X, DodgeDir.Y, DodgeDir.Z, CharacterOwner->GetActorRotation().Yaw);
*/
	float HorizontalImpulse = DodgeImpulseHorizontal;
	bool bIsLowGrav = !bApplyWallSlide && (GetGravityZ() > UPhysicsSettings::Get()->DefaultGravityZ);

	if (!IsMovingOnGround())
	{
		if (IsFalling() && (CurrentWallDodgeCount >= MaxWallDodges))
		{
			return false;
		}
		// if falling/swimming, check if can perform wall dodge
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
		if (!bBlockingHit || (!IsSwimming() && (CurrentWallDodgeCount > 0) && !bIsLowGrav && ((Result.ImpactNormal | LastWallDodgeNormal) > MaxConsecutiveWallDodgeDP)))
		{
			return false;
		}
		if ((Result.ImpactNormal | DodgeDir) < WallDodgeMinNormal)
		{
			// clamp dodge direction based on wall normal
			FVector ForwardDir = (Result.ImpactNormal ^ FVector(0.f, 0.f, 1.f)).SafeNormal();
			if ((ForwardDir | DodgeDir) < 0.f)
			{
				ForwardDir *= -1.f;
			}
			DodgeDir = Result.ImpactNormal*WallDodgeMinNormal*WallDodgeMinNormal + ForwardDir*(1.f - WallDodgeMinNormal*WallDodgeMinNormal);
			DodgeDir = DodgeDir.SafeNormal();
			FVector NewDodgeCross = (DodgeDir ^ FVector(0.f, 0.f, 1.f)).SafeNormal();
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
	}

	// perform the dodge
	float VelocityZ = Velocity.Z;
	Velocity = HorizontalImpulse*DodgeDir + (Velocity | DodgeCross)*DodgeCross;
	Velocity.Z = 0.f;
	float SpeedXY = FMath::Min(Velocity.Size(), DodgeMaxHorizontalVelocity);
	Velocity = SpeedXY*Velocity.SafeNormal();
	if (IsMovingOnGround())
	{
		Velocity.Z = DodgeImpulseVertical;
		CurrentMultiJumpCount++;
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
		if (bCountWallDodgeMultijumps)
		{
			CurrentMultiJumpCount++;
		}
		//UE_LOG(UT, Warning, TEXT("Wall dodge at %f velZ %f"), CharacterOwner->GetWorld()->GetTimeSeconds(), Velocity.Z);
	}
	else
	{
		Velocity.Z = VelocityZ;
		if (bCountWallDodgeMultijumps)
		{
			CurrentMultiJumpCount++;
		}
	}
	bIsDodging = true;
	bNotifyApex = true;
	bJustDodged = true;
	if (IsMovingOnGround())
	{
		SetMovementMode(MOVE_Falling);
	}
	return true;
}

FVector UUTCharacterMovement::ConsumeInputVector()
{
	FVector NewInputVector = Super::ConsumeInputVector();

	if (bWantsSlideRoll && NewInputVector.IsZero() && bMaintainSlideRollAccel)
	{
		NewInputVector = SavedAcceleration;
	}

	SavedAcceleration = NewInputVector;
	return NewInputVector;
}

void UUTCharacterMovement::Crouch(bool bClientSimulation)
{
	float RealCrouchHeight = CrouchedHalfHeight;
	CrouchedHalfHeight = bIsDodgeRolling ? RollHalfHeight : CrouchedHalfHeight;
	Super::Crouch(bClientSimulation);
	CrouchedHalfHeight = RealCrouchHeight;
}

void UUTCharacterMovement::PerformMovement(float DeltaSeconds)
{
	if (!CharacterOwner)
	{
		return;
	}
	OldZ = CharacterOwner->GetActorLocation().Z;

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
/*
	FVector Loc = CharacterOwner->GetActorLocation();
	float CurrentMoveTime = GetCurrentSynchTime();
	if (CharacterOwner->Role < ROLE_Authority)
	{
		UE_LOG(UT, Warning, TEXT("CLIENT MOVE at %f from %f %f %f vel %f %f %f"), CurrentMoveTime, Loc.X, Loc.Y, Loc.Z, Velocity.X, Velocity.Y, Velocity.Z);
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("SERVER Move at %f from %f %f %f vel %f %f %f"), CurrentMoveTime, Loc.X, Loc.Y, Loc.Z, Velocity.X, Velocity.Y, Velocity.Z);
	}
*/
	//UE_LOG(UT, Warning, TEXT("PerformMovement %f saved sprint start %f bIsSprinting %d"), GetCurrentMovementTime(), SprintStartTime, bIsSprinting);

	Super::PerformMovement(DeltaSeconds);
	bWantsToCrouch = bSavedWantsToCrouch;
	GroundFriction = RealGroundFriction;

	if (Cast<AUTCharacter>(CharacterOwner))
	{
		((AUTCharacter*)CharacterOwner)->PositionUpdated();
	}
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
	if (CharacterOwner && IsMovingOnGround() && !IsCrouching() && (GetCurrentMovementTime() > SprintStartTime)) 
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
	else if (bIsEmoting)
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
	if (Acceleration.IsZero())
	{
		SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

float UUTCharacterMovement::GetCurrentMovementTime() const
{
	// @TODO FIXMESTEVE remove listen server support here
	return ((CharacterOwner->Role == ROLE_AutonomousProxy) || (GetNetMode() == NM_DedicatedServer) || ((GetNetMode() == NM_ListenServer) && !CharacterOwner->IsLocallyControlled()))
		? CurrentServerMoveTime
		: CharacterOwner->GetWorld()->GetTimeSeconds();
}

float UUTCharacterMovement::GetCurrentSynchTime() const
{
	if (CharacterOwner->Role < ROLE_Authority)
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData)
		{
			return ClientData->CurrentTimeStamp;
		}
	}
	return GetCurrentMovementTime();
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
		if (CurrentServerMoveTime > ClientTimeStamp + 0.5f*MinTimeBetweenTimeStampResets)
		{
			// client timestamp rolled over, so roll over our movement timers
			//UE_LOG(UT, Warning, TEXT("+++++++ROLLOVER time %f"), CurrentServerMoveTime); //MinTimeBetweenTimeStampResets
			DodgeResetTime -= MinTimeBetweenTimeStampResets;
			SprintStartTime -= MinTimeBetweenTimeStampResets;
			DodgeRollTapTime -= MinTimeBetweenTimeStampResets;
			DodgeRollEndTime -= MinTimeBetweenTimeStampResets;
		}
		CurrentServerMoveTime = ClientTimeStamp;
		//UE_LOG(UT, Warning, TEXT("+++++++Set server move time %f"), CurrentServerMoveTime); //MinTimeBetweenTimeStampResets
	}
	if (!HasValidData())
	{
		return;
	}

	UpdateFromCompressedFlags(CompressedFlags);
	//UE_LOG(UT, Warning, TEXT("sprinting %d acceleration %f %f"), bIsSprinting, Acceleration.X, Acceleration.Y);
	bool bOldSprinting = bIsSprinting;
	FVector OldAccel = NewAccel;
	CharacterOwner->CheckJumpInput(DeltaTime);
	Acceleration = ConstrainInputAcceleration(NewAccel);
	Acceleration = ScaleInputAcceleration(Acceleration);

	AnalogInputModifier = ComputeAnalogInputModifier();
/*
	if (bOldSprinting != bIsSprinting)
	{
		UE_LOG(UT, Warning, TEXT("%f SPRINTING changed from %d to %d"), ClientTimeStamp, bOldSprinting, bIsSprinting);

	}*/
	PerformMovement(DeltaTime);

	// If not playing root motion, tick animations after physics. We do this here to keep events, notifies, states and transitions in sync with client updates.
	if (!CharacterOwner->bClientUpdating && !CharacterOwner->IsPlayingRootMotion() && CharacterOwner->Mesh)
	{
		TickCharacterPose(DeltaTime);
		// TODO: SaveBaseLocation() in case tick moves us?
	}
}

void UUTCharacterMovement::ResetTimers()
{
	DodgeResetTime = 0.f;
	SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
	DodgeRollTapTime = 0.f;
	DodgeRollEndTime = 0.f;
}

float UUTCharacterMovement::FallingDamageReduction(float FallingDamage, const FHitResult& Hit)
{
	if (Hit.ImpactNormal.Z < GetWalkableFloorZ())
	{
		// Scale damage based on angle of wall we hit
		return FallingDamage * Hit.ImpactNormal.Z;
	}
	return (GetCurrentMovementTime() - DodgeRollTapTime < DodgeRollBonusTapInterval) ? FallingDamageRollReduction : 0.f;
}

void UUTCharacterMovement::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	bIsAgainstWall = false;

	if (CharacterOwner)
	{
		bIsDodgeRolling = bWantsSlideRoll;
		if (CharacterOwner->ShouldNotifyLanded(Hit))
		{
			CharacterOwner->Landed(Hit);
		}
		if (bIsDodgeRolling)
		{
			DodgeRollEndTime = GetCurrentMovementTime() + DodgeRollDuration;
			Acceleration = DodgeRollAcceleration * Velocity.SafeNormal2D();
			//UE_LOG(UT, Warning, TEXT("DodgeRoll within %f"), GetCurrentMovementTime() - DodgeRollTapTime);
			// @TODO FIXMESTEVE - should also update DodgeRestTime if roll but not out of dodge?
			if (bIsDodging)
			{
				DodgeResetTime = DodgeRollEndTime + ((CurrentMultiJumpCount > 1) ? DodgeJumpResetInterval : DodgeResetInterval);
				//UE_LOG(UT, Warning, TEXT("Set dodge reset after landing move time %f dodge reset time %f"), GetCurrentMovementTime(), DodgeResetTime);
			}
		}
		else if (bIsDodging)
		{
			Velocity *= ((CurrentMultiJumpCount > 1) ? DodgeJumpLandingSpeedFactor : DodgeLandingSpeedFactor);
			DodgeResetTime = GetCurrentMovementTime() + ((CurrentMultiJumpCount > 1) ? DodgeJumpResetInterval : DodgeResetInterval);
		}
		bIsDodging = false;
		SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
	}
	bJumpAssisted = false;
	bApplyWallSlide = false;
	CurrentMultiJumpCount = 0;
	CurrentWallDodgeCount = 0;
	if (IsFalling())
	{
		SetPostLandedPhysics(Hit);
	}

	StartNewPhysics(remainingTime, Iterations);
}

void UUTCharacterMovement::UpdateSlideRoll(bool bNewWantsSlideRoll)
{
	if (bNewWantsSlideRoll && !bWantsSlideRoll)
	{
		DodgeRollTapTime = GetCurrentMovementTime();
	}
	bWantsSlideRoll = bNewWantsSlideRoll;
}

bool UUTCharacterMovement::WantsSlideRoll()
{ 
	return bWantsSlideRoll; 
}

void UUTCharacterMovement::PerformRoll(const FVector& DodgeDir)
{
	if (!IsFalling() && CharacterOwner)
	{
		// @todo FIXMESTEVE test networking
		DodgeRollTapTime = GetCurrentMovementTime();
		bIsDodgeRolling = true;
		DodgeRollEndTime = GetCurrentMovementTime() + DodgeRollDuration;
		Acceleration = DodgeRollAcceleration * DodgeDir;
		DodgeResetTime = DodgeRollEndTime + DodgeResetInterval;
		Velocity = MaxDodgeRollSpeed*DodgeDir;
		UUTGameplayStatics::UTPlaySound(GetWorld(), Cast<AUTCharacter>(CharacterOwner)->DodgeRollSound, CharacterOwner, SRT_None);
	}
}

bool UUTCharacterMovement::DoJump(bool bReplayingMoves)
{
	if (CharacterOwner && CharacterOwner->CanJump() && (IsFalling() ? DoMultiJump() : Super::DoJump(bReplayingMoves)))
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
		Velocity.Z = bIsDodging ? DodgeJumpImpulse : MultiJumpImpulse;
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
	return ((MaxMultiJumpCount > 1) && (CurrentMultiJumpCount < MaxMultiJumpCount) && (!bIsDodging || bAllowDodgeMultijumps) && (bIsDodging || bAllowJumpMultijumps) && (FMath::Abs(Velocity.Z) < MaxMultiJumpZSpeed));
}

void UUTCharacterMovement::ClearDodgeInput()
{
	bPressedDodgeForward = false;
	bPressedDodgeBack = false;
	bPressedDodgeLeft = false;
	bPressedDodgeRight = false;
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
			UTCharacterOwner->Dodge((DodgeDirX*X + DodgeDirY*Y).SafeNormal(), (DodgeCrossX*X + DodgeCrossY*Y).SafeNormal());
		}
	}

	if (CharacterOwner)
	{
		// If server, we already got these flags from the saved move
		if (CharacterOwner->IsLocallyControlled())
		{
			bIsDodgeRolling = bIsDodgeRolling && (GetCurrentMovementTime() < DodgeRollEndTime);
			bIsSprinting = CanSprint();
		}
		bJustDodged = false;

		if (!bIsDodgeRolling && bWasDodgeRolling)
		{
			SprintStartTime = GetCurrentMovementTime() + AutoSprintDelayInterval;
		}
	}

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
	if ((bSavedIsRolling != ((FSavedMove_UTCharacter*)&NewMove)->bSavedIsRolling))
	{
		return false;
	}
	if (bSavedWantsSlide != ((FSavedMove_UTCharacter*)&NewMove)->bSavedWantsSlide)
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

	if (bSavedWantsSlide)
	{
		Result |= FLAG_Custom_1;
	}

	if (bSavedIsEmoting)
	{
		Result |= FLAG_Custom_2;
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
	bSavedWantsSlide = false;
	bSavedIsEmoting = false;
	SavedMultiJumpCount = 0;
	SavedWallDodgeCount = 0;
	SavedSprintStartTime = 0.f;
	SavedDodgeResetTime = 0.f;
	SavedDodgeRollEndTime = 0.f;
	bSavedJumpAssisted = false;
	bSavedIsDodging = false;
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
		bSavedWantsSlide = UTCharMov->WantsSlideRoll(); 
		bSavedIsEmoting = UTCharMov->bIsEmoting;
		SavedMultiJumpCount = UTCharMov->CurrentMultiJumpCount;
		SavedWallDodgeCount = UTCharMov->CurrentWallDodgeCount;
		SavedSprintStartTime = UTCharMov->SprintStartTime;
		SavedDodgeResetTime = UTCharMov->DodgeResetTime;
		SavedDodgeRollEndTime = UTCharMov->DodgeRollEndTime;
		bSavedJumpAssisted = UTCharMov->bJumpAssisted;
		bSavedIsDodging = UTCharMov->bIsDodging;
		//UE_LOG(UT, Warning, TEXT("set move %f saved sprint start %f"), TimeStamp, SavedSprintStartTime);
	}

	// Round acceleration, so sent version and locally used version always match
	Acceleration.X = FMath::RoundToFloat(Acceleration.X);
	Acceleration.Y = FMath::RoundToFloat(Acceleration.Y);
	Acceleration.Z = FMath::RoundToFloat(Acceleration.Z);
}

void FSavedMove_UTCharacter::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	UUTCharacterMovement* UTCharMov = Cast<UUTCharacterMovement>(Character->CharacterMovement);
	if (UTCharMov)
	{
		if (UTCharMov->bIsSettingUpFirstReplayMove)
		{
			UTCharMov->CurrentMultiJumpCount = SavedMultiJumpCount;
			UTCharMov->CurrentWallDodgeCount = SavedWallDodgeCount;
			UTCharMov->SprintStartTime = SavedSprintStartTime;
			UTCharMov->DodgeResetTime = SavedDodgeResetTime;
			UTCharMov->DodgeRollEndTime = SavedDodgeRollEndTime;
			UTCharMov->bJumpAssisted = bSavedJumpAssisted;
			UTCharMov->bIsDodging = bSavedIsDodging;
			//UE_LOG(UT, Warning, TEXT("First move %f saved sprint start %f"), TimeStamp, UTCharMov->SprintStartTime);
		}
		else
		{
/*
			// warn if any of these changed (can be legit)
			if (SavedMultiJumpCount != UTCharMov->CurrentMultiJumpCount)
			{
				UE_LOG(UT, Warning, TEXT("prep move %f SavedMultiJumpCount from %d to %d"), TimeStamp, SavedMultiJumpCount, UTCharMov->CurrentMultiJumpCount);
			}
			if (SavedWallDodgeCount != UTCharMov->CurrentWallDodgeCount)
			{
				UE_LOG(UT, Warning, TEXT("prep move %f SavedWallDodgeCount from %d to %d"), TimeStamp, SavedWallDodgeCount, UTCharMov->CurrentWallDodgeCount);
			}
			if (SavedSprintStartTime != UTCharMov->SprintStartTime)
			{
				UE_LOG(UT, Warning, TEXT("prep move %f SavedSprintStartTime from %f to %f"), TimeStamp, SavedSprintStartTime, UTCharMov->SprintStartTime);
			}
			if (SavedDodgeResetTime != UTCharMov->DodgeResetTime)
			{
				UE_LOG(UT, Warning, TEXT("prep move %f SavedDodgeResetTime from %f to %f"), TimeStamp, SavedDodgeResetTime, UTCharMov->DodgeResetTime);
			}
			if (SavedDodgeRollEndTime != UTCharMov->DodgeRollEndTime)
			{
				UE_LOG(UT, Warning, TEXT("prep move %f SavedDodgeResetTime from %f to %f"), TimeStamp, SavedDodgeRollEndTime, UTCharMov->DodgeRollEndTime);
			}
			if (SavedWallDodgeCount != UTCharMov->CurrentWallDodgeCount)
			{
				UE_LOG(UT, Warning, TEXT("prep move %f SavedWallDodgeCount from %d to %d"), TimeStamp, SavedWallDodgeCount, UTCharMov->CurrentWallDodgeCount);
			}
			if (bSavedJumpAssisted != UTCharMov->bJumpAssisted)
			{
				UE_LOG(UT, Warning, TEXT("prep move %f bSavedJumpAssisted from %d to %d"), TimeStamp, bSavedJumpAssisted, UTCharMov->bJumpAssisted);
			}
			if (bSavedIsDodging != UTCharMov->bIsDodging)
			{
				UE_LOG(UT, Warning, TEXT("prep move %f bSavedIsDodging from %d to %d"), TimeStamp, bSavedIsDodging, UTCharMov->bIsDodging);
			}
*/
			// these may have changed in the course of replaying saved moves.  Save new value, since we reset to this before the first one is played
			// @TODO FIXMESTEVE should I update all properties (non input)?
			SavedMultiJumpCount = UTCharMov->CurrentMultiJumpCount;
			SavedWallDodgeCount = UTCharMov->CurrentWallDodgeCount;
			SavedSprintStartTime = UTCharMov->SprintStartTime;
			SavedDodgeResetTime = UTCharMov->DodgeResetTime;
			SavedDodgeRollEndTime = UTCharMov->DodgeRollEndTime;
			bSavedJumpAssisted = UTCharMov->bJumpAssisted;
			bSavedIsDodging = UTCharMov->bIsDodging;
		}
	}
}

void FSavedMove_UTCharacter::PostUpdate(ACharacter* Character, FSavedMove_Character::EPostUpdateMode PostUpdateMode)
{
	Super::PostUpdate(Character, PostUpdateMode);

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
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f; // 2X character capsule radius
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}

	return ClientPredictionData;
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
		CharacterOwner->CapsuleComponent->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
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
	// @TODO FIXMESTEVE Temp hack until we can get the crouch control code split out from PerformMovement()
	if (IsCrouching() && !bIsDodgeRolling && (CharacterOwner->CapsuleComponent->GetScaledCapsuleHalfHeight() < CrouchedHalfHeight))
	{
		return false;
	}
	return CanEverCrouch() && IsMovingOnGround();
}

void UUTCharacterMovement::CheckWallSlide(FHitResult const& Impact)
{
	bApplyWallSlide = false;
	if ((bWantsSlideRoll || bAutoSlide) && (Velocity.Z < 0.f) && (CurrentMultiJumpCount > 0) && (Velocity.Z > MaxSlideFallZ) && !Acceleration.IsZero() && ((Acceleration.SafeNormal() | Impact.ImpactNormal) < MaxSlideAccelNormal))
	{
		FVector VelocityAlongWall = Velocity + (Velocity | Impact.ImpactNormal);
		bApplyWallSlide = (VelocityAlongWall.Size2D() >= MinWallSlideSpeed);
		if (bApplyWallSlide && Cast<AUTCharacter>(CharacterOwner) && Cast<AUTCharacter>(CharacterOwner)->bCanPlayWallHitSound)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), Cast<AUTCharacter>(CharacterOwner)->WallHitSound, CharacterOwner, SRT_None);
			Cast<AUTCharacter>(CharacterOwner)->bCanPlayWallHitSound = false;
		}
	}
}

void UUTCharacterMovement::HandleImpact(FHitResult const& Impact, float TimeSlice, const FVector& MoveDelta)
{
	if (IsFalling())
	{
		CheckWallSlide(Impact);
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
	return Super::GetGravityZ() * (bApplyWallSlide ? SlideGravityScaling : 1.f);
}

void UUTCharacterMovement::PhysSwimming(float deltaTime, int32 Iterations)
{
	if (Velocity.Size() > MaxWaterSpeed)
	{
		FVector VelDir = Velocity.SafeNormal();
		if ((VelDir | Acceleration) > 0.f)
		{
			Acceleration = Acceleration - (VelDir | Acceleration)*VelDir; 
		}
	}
	Super::PhysSwimming(deltaTime, Iterations);
}

float UUTCharacterMovement::ComputeAnalogInputModifier() const
{
	return 1.f;
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

	if ((CurrentWallDodgeCount > 0) && (Velocity.Z > 0.f) && ((FallAcceleration | LastWallDodgeNormal) < 0.f) && ((FallAcceleration.SafeNormal() | LastWallDodgeNormal) < -1.f*WallDodgeMinNormal))
	{
		// don't air control back into wall you just dodged from  
		FallAcceleration = FallAcceleration - (FallAcceleration | LastWallDodgeNormal) * LastWallDodgeNormal;
		FallAcceleration = FallAcceleration.SafeNormal();
	}
	bool bSkipLandingAssist = true;
	bApplyWallSlide = false;
	FHitResult Hit(1.f);
	bIsAgainstWall = false;
	if (!HasRootMotion())
	{
	// test for slope to avoid using air control to climb walls 
		float TickAirControl = (CurrentMultiJumpCount - CurrentWallDodgeCount  < 2) ? AirControl : MultiJumpAirControl;
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
						// We are against the wall, store info about it
						bIsAgainstWall = true;
						
						FVector Lat = FVector::CrossProduct(CharacterOwner->GetActorRotation().Vector(), FVector(0,0,1));
						WallDirection = FVector::DotProduct(Lat, Result.Normal);

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

		FallAcceleration = FallAcceleration.ClampMaxSize(MaxAccel);
	}

	float remainingTime = deltaTime;
	float timeTick = 0.1f;

/*
	FVector Loc = CharacterOwner->GetActorLocation();
	float CurrentMoveTime = GetCurrentSynchTime();
	if (CharacterOwner->Role < ROLE_Authority)
	{
		UE_LOG(UT, Warning, TEXT("CLIENT Fall at %f from %f %f %f vel %f %f %f delta %f"), ClientData->CurrentTimeStamp, Loc.X, Loc.Y, Loc.Z, Velocity.X, Velocity.Y, Velocity.Z, deltaTime);
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("SERVER Fall at %f from %f %f %f vel %f %f %f delta %f"), GetCurrentMovementTime(), Loc.X, Loc.Y, Loc.Z, Velocity.X, Velocity.Y, Velocity.Z, deltaTime);
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
			bApplyWallSlide = false;
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
					bApplyWallSlide = false;
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
							bApplyWallSlide = false;
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
		bApplyWallSlide = false;

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
/*
		FVector Loc = CharacterOwner->GetActorLocation();
		float CurrentMoveTime = GetCurrentSynchTime();
		if (CharacterOwner->Role < ROLE_Authority)
		{
			UE_LOG(UT, Warning, TEXT("FINAL VELOCITY at %f vel %f %f %f"), CurrentMoveTime, Velocity.X, Velocity.Y, Velocity.Z);
		}
		else
		{
			UE_LOG(UT, Warning, TEXT("FINAL VELOCITY at %f vel %f %f %f"), CurrentMoveTime, Velocity.X, Velocity.Y, Velocity.Z);

		}
*/
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
	bool bHit = GetWorld()->SweepSingle(Result, PawnLocation, PawnLocation + FVector(0.f, 0.f,LandingStepUp), FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);
	FVector HorizontalStart = bHit ? Result.Location : PawnLocation + FVector(0.f, 0.f, LandingStepUp);
	float ElapsedTime = 0.05f; // FMath::Min(0.05f, remainingTime);
	FVector HorizontalDir = Acceleration.SafeNormal2D() * MaxWalkSpeed * 0.05f;;
	bHit = GetWorld()->SweepSingle(Result, HorizontalStart, HorizontalStart + HorizontalDir, FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);
	FVector LandingStart = bHit ? Result.Location : HorizontalStart + HorizontalDir;
	bHit = GetWorld()->SweepSingle(Result, LandingStart, LandingStart - FVector(0.f, 0.f, LandingStepUp), FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);

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

void UUTCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bIsEmoting = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;

	int32 DodgeFlags = (Flags >> 2) & 7;
	bPressedDodgeForward = (DodgeFlags == 1);
	bPressedDodgeBack = (DodgeFlags == 2);
	bPressedDodgeLeft = (DodgeFlags == 3);
	bPressedDodgeRight = (DodgeFlags == 4);
	bIsSprinting = (DodgeFlags == 5);
	bIsDodgeRolling = (DodgeFlags == 6);
	bool bOldWillDodgeRoll = bWantsSlideRoll;
	bWantsSlideRoll = ((Flags & FSavedMove_Character::FLAG_Custom_1) != 0);
	if (!bOldWillDodgeRoll && bWantsSlideRoll)
	{
		DodgeRollTapTime = GetCurrentMovementTime();
	}
	if (Cast<AUTCharacter>(CharacterOwner))
	{
		Cast<AUTCharacter>(CharacterOwner)->bRepDodgeRolling = bIsDodgeRolling;
	}
}

void UUTCharacterMovement::ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode)
{
	// use normal replication when simulating physics
	if (CharacterOwner == NULL || CharacterOwner->GetRootComponent() == NULL || !CharacterOwner->GetRootComponent()->IsSimulatingPhysics())
	{
		Super::ClientAdjustPosition_Implementation(TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);
	}
}

void UUTCharacterMovement::SmoothClientPosition(float DeltaSeconds)
{
	if (!HasValidData() || GetNetMode() != NM_Client)
	{
		return;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (ClientData && ClientData->bSmoothNetUpdates)
	{
		// smooth interpolation of mesh translation to avoid popping of other client pawns, unless driving or ragdoll or low tick rate
		if ((DeltaSeconds < ClientData->SmoothNetUpdateTime) && CharacterOwner->Mesh && !CharacterOwner->Mesh->IsSimulatingPhysics())
		{
			float SmoothTime = Velocity.IsZero() ? 0.05f : ClientData->SmoothNetUpdateTime;
			ClientData->MeshTranslationOffset = (ClientData->MeshTranslationOffset * (1.f - DeltaSeconds / SmoothTime));
		}
		else
		{
			ClientData->MeshTranslationOffset = FVector::ZeroVector;
		}

		if (IsMovingOnGround())
		{
			// don't smooth Z position if walking on ground
			ClientData->MeshTranslationOffset.Z = 0;
		}

		if (CharacterOwner->Mesh)
		{
			const FVector NewRelTranslation = CharacterOwner->ActorToWorld().InverseTransformVectorNoScale(ClientData->MeshTranslationOffset + CharacterOwner->GetBaseTranslationOffset());
			CharacterOwner->Mesh->SetRelativeLocation(NewRelTranslation);
		}
		//DrawDebugSphere(GetWorld(), CharacterOwner->GetActorLocation(), 30.f, 8, FColor::Yellow);
	}
}

void UUTCharacterMovement::SimulateMovement(float DeltaSeconds)
{
	if (!HasValidData() || UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	bool bWasFalling = (MovementMode == MOVE_Falling);
	FVector RealVelocity = Velocity; // Remove if we start using actual acceleration.  Used now to keep our forced clientside decel from affecting animation

	float RemainingTime = DeltaSeconds;
	while (RemainingTime > 0.001f)
	{
		Velocity = SimulatedVelocity;
		float DeltaTime = RemainingTime;
		if (RemainingTime > MaxSimulationTimeStep)
		{
			DeltaTime = FMath::Min(0.5f*RemainingTime, MaxSimulationTimeStep);
		}
		RemainingTime -= DeltaTime;
		Super::SimulateMovement(DeltaTime);

		if (CharacterOwner->Role == ROLE_SimulatedProxy)
		{
			// update velocity with replicated acceleration - handle falling also
			// For now, pretend 0 accel if walking on ground
			if (MovementMode != MOVE_Falling)
			{
				bHasCheckedAgainstWall = false;
			}
			if (MovementMode == MOVE_Walking)
			{
				bIsAgainstWall = false;
				float Speed = Velocity.Size();
				if (bWasFalling && (Speed > MaxWalkSpeed))
				{
					if (Speed > SprintSpeed)
					{
						SimulatedVelocity = DodgeLandingSpeedFactor * Velocity.SafeNormal2D();
					}
					else
					{
						SimulatedVelocity = MaxWalkSpeed * Velocity.SafeNormal2D();
					}
				}
				else if (Speed > 0.5f * MaxWalkSpeed)
				{
					SimulatedVelocity *= (1.f - 2.f*DeltaTime);
				}
			}
			// @TODO FIXMESTEVE - need to update falling velocity after simulate also
		}
	}
	Velocity = RealVelocity;
}


void UUTCharacterMovement::MoveSmooth(const FVector& InVelocity, const float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	//@TODO FIXMESTEVE - really just want MoveSmooth() to add a hit notification
	if ((MovementMode != MOVE_Falling) || !HasValidData() || (CharacterOwner->Role != ROLE_SimulatedProxy))
	{
		Super::MoveSmooth(InVelocity, DeltaSeconds, OutStepDownResult);
		return;
	}
	FVector Delta = InVelocity * DeltaSeconds;
	if (Delta.IsZero())
	{
		return;
	}

	FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, CharacterOwner->GetActorRotation(), true, Hit);

	if (Hit.IsValidBlockingHit())
	{
		bIsAgainstWall = true;
		FVector Lat = FVector::CrossProduct(CharacterOwner->GetActorRotation().Vector(), FVector(0,0,1));
		WallDirection = FVector::DotProduct(Lat, Hit.Normal);

		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, false);
	}
	else if (!bHasCheckedAgainstWall)
	{
		bHasCheckedAgainstWall = true;
		static const FName FallingTraceParamsTag = FName(TEXT("PhysFalling"));
		const float TestWalkTime = FMath::Max(DeltaSeconds, 0.05f);
		const FVector TestWalk = (FVector(0.f, 0.f, GetGravityZ()) * TestWalkTime + Velocity) * TestWalkTime;
		FCollisionQueryParams CapsuleQuery(FallingTraceParamsTag, false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(CapsuleQuery, ResponseParam);
		const FVector PawnLocation = CharacterOwner->GetActorLocation();
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
		const bool bHit = GetWorld()->SweepSingle(Hit, PawnLocation, PawnLocation + TestWalk, FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);
		if (bHit)
		{
			bIsAgainstWall = true;

			FVector Lat = FVector::CrossProduct(CharacterOwner->GetActorRotation().Vector(), FVector(0,0,1));
			WallDirection = FVector::DotProduct(Lat, Hit.Normal);

		}
	}
}

void UUTCharacterMovement::SendClientAdjustment()
{
	if (!HasValidData())
	{
		return;
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (ServerData->PendingAdjustment.TimeStamp <= 0.f)
	{
		return;
	}

	if (ServerData->PendingAdjustment.bAckGoodMove && Cast<AUTCharacter>(CharacterOwner))
	{
		Cast<AUTCharacter>(CharacterOwner)->GoodMoveAckTime = ServerData->PendingAdjustment.TimeStamp;
		ServerData->PendingAdjustment.TimeStamp = 0;
		ServerData->PendingAdjustment.bAckGoodMove = false;
	}
	else
	{
		Super::SendClientAdjustment();
	}
}

// @TODO FIXMESTEVE - remove this once engine implements CanDelaySendingMove()
void UUTCharacterMovement::ReplicateMoveToServer(float DeltaTime, const FVector& NewAcceleration)
{
	check(CharacterOwner != NULL);

	// Can only start sending moves if our controllers are synced up over the network, otherwise we flood the reliable buffer.
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (PC && PC->AcknowledgedPawn != CharacterOwner)
	{
		return;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (!ClientData)
	{
		return;
	}

	// Find the oldest (unacknowledged) important move (OldMove).
	// Don't include the last move because it may be combined with the next new move.
	// A saved move is interesting if it differs significantly from the last acknowledged move
	FSavedMovePtr OldMove = NULL;
	if (ClientData->LastAckedMove.IsValid())
	{
		for (int32 i = 0; i < ClientData->SavedMoves.Num() - 1; i++)
		{
			const FSavedMovePtr& CurrentMove = ClientData->SavedMoves[i];
			if (CurrentMove->IsImportantMove(ClientData->LastAckedMove))
			{
				OldMove = CurrentMove;
				break;
			}
		}
	}

	// Get a SavedMove object to store the movement in.
	FSavedMovePtr NewMove = ClientData->CreateSavedMove();
	if (NewMove.IsValid() == false)
	{
		return;
	}

	NewMove->SetMoveFor(CharacterOwner, DeltaTime, NewAcceleration, *ClientData);
	NewMove->SetInitialPosition(CharacterOwner);

	// @TODO FIXMESTEVE re-introduce limited move combining

	// Perform the move locally
	Acceleration = ScaleInputAcceleration(NewMove->Acceleration);
	CharacterOwner->ClientRootMotionParams.Clear();
	PerformMovement(NewMove->DeltaTime);

	NewMove->PostUpdate(CharacterOwner, FSavedMove_Character::PostUpdate_Record);

	// Add NewMove to the list
	ClientData->SavedMoves.Push(NewMove);

	if (CanDelaySendingMove(NewMove) && ClientData->PendingMove.IsValid() == false)
	{
		// Decide whether to hold off on move
		// send moves more frequently in small games where server isn't likely to be saturated
		float NetMoveDelta;
		UPlayer* Player = (PC ? PC->Player : NULL);

		if (Player && (Player->CurrentNetSpeed > 10000) && (GetWorld()->GameState != NULL) && (GetWorld()->GameState->PlayerArray.Num() <= 10))
		{
			NetMoveDelta = 0.011f;
		}
		else if (Player && CharacterOwner->GetWorldSettings()->GameNetworkManagerClass)
		{
			NetMoveDelta = FMath::Max(0.0222f, 2 * GetDefault<AGameNetworkManager>(CharacterOwner->GetWorldSettings()->GameNetworkManagerClass)->MoveRepSize / Player->CurrentNetSpeed);
		}
		else
		{
			NetMoveDelta = 0.011f;
		}

		if ((GetWorld()->TimeSeconds - ClientData->ClientUpdateTime) * CharacterOwner->GetWorldSettings()->GetEffectiveTimeDilation() < NetMoveDelta)
		{
			ClientData->PendingMove = NewMove;
			return;
		}
	}

	ClientData->ClientUpdateTime = GetWorld()->TimeSeconds;

	UE_LOG(LogNetPlayerMovement, Verbose, TEXT("Client ReplicateMove Time %f Acceleration %s Position %s DeltaTime %f"),
		NewMove->TimeStamp, *NewMove->Acceleration.ToString(), *CharacterOwner->GetActorLocation().ToString(), DeltaTime);

	// Send to the server
	CallServerMove(NewMove.Get(), OldMove.Get());
	ClientData->PendingMove = NULL;
}

bool UUTCharacterMovement::CanDelaySendingMove(const FSavedMovePtr& NewMove)
{
	if (true) // @TODO FIXMESTEVE CVarNetEnableMoveCombining.GetValueOnGameThread() != 0)  
	{
		// don't delay if just dodged - important because combined moves don't send first rotation
		if (bJustDodged)
		{
			return false;
		}

		// don't delay if just pressed fire
		AUTPlayerController * PC = CharacterOwner ? Cast<AUTPlayerController>(CharacterOwner->GetController()) : NULL;
		if (PC && (PC->HasDeferredFireInputs()))
		{
			return false;
		}
		return true;
	}
	return false;
}

bool FSavedMove_UTCharacter::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	if (bPressedDodgeForward || bPressedDodgeBack || bPressedDodgeLeft || bPressedDodgeRight)
	{
		return true;
	}
	return Super::IsImportantMove(LastAckedMove);
}


void UUTCharacterMovement::CallServerMove
(
const class FSavedMove_Character* NewMove,
const class FSavedMove_Character* OldMove
)
{
	check(NewMove != NULL);

	AUTCharacter* UTCharacterOwner = Cast<AUTCharacter>(CharacterOwner);
	if (!UTCharacterOwner)
	{
		UE_LOG(UT, Warning, TEXT("CallServerMove() with no owner!?!"));
		return;
	}

	// Determine if we send absolute or relative location
	UPrimitiveComponent* ClientMovementBase = NewMove->EndBase.Get();
	const FName ClientBaseBone = NewMove->EndBoneName;
	const FVector SendLocation = MovementBaseUtility::UseRelativeLocation(ClientMovementBase) ? NewMove->SavedRelativeLocation : NewMove->SavedLocation;

	// send old move if it exists
	if (OldMove)
	{
		UTCharacterOwner->UTServerMoveOld(OldMove->TimeStamp, OldMove->Acceleration, OldMove->SavedControlRotation.Yaw, OldMove->GetCompressedFlags());
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (ClientData->PendingMove.IsValid())
	{
		// send two moves simultaneously
		UTCharacterOwner->UTServerMoveDual
			(
			ClientData->PendingMove->TimeStamp,
			ClientData->PendingMove->Acceleration,
			ClientData->PendingMove->GetCompressedFlags(),
			NewMove->TimeStamp,
			NewMove->Acceleration,
			SendLocation,
			NewMove->GetCompressedFlags(),
			NewMove->SavedControlRotation.Yaw,
			NewMove->SavedControlRotation.Pitch,
			ClientMovementBase,
			ClientBaseBone,
			PackNetworkMovementMode()
			);
	}
	else
	{
		UTCharacterOwner->UTServerMove
			(
			NewMove->TimeStamp,
			NewMove->Acceleration,
			SendLocation,
			NewMove->GetCompressedFlags(),
			NewMove->SavedControlRotation.Yaw,
			NewMove->SavedControlRotation.Pitch,
			ClientMovementBase,
			ClientBaseBone,
			PackNetworkMovementMode()
			);
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	APlayerCameraManager* PlayerCameraManager = (PC ? PC->PlayerCameraManager : NULL);
	if (PlayerCameraManager != NULL && PlayerCameraManager->bUseClientSideCameraUpdates)
	{
		//UE_LOG(UT, Warning, TEXT("WTF WTF WTF WTF!!!!!!!!!!!!!!!!"));
		PlayerCameraManager->bShouldSendClientSideCameraUpdate = true;
	}
}

void UUTCharacterMovement::ProcessServerMove(float TimeStamp, FVector InAccel, FVector ClientLoc, uint8 MoveFlags, float ViewYaw, float ViewPitch, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	if (!HasValidData() || !IsComponentTickEnabled())
	{
		return;
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (!VerifyClientTimeStamp(TimeStamp, *ServerData))
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	bool bServerReadyForClient = PC ? PC->NotifyServerReceivedClientData(CharacterOwner, TimeStamp) : true;
	const FVector Accel = bServerReadyForClient ? InAccel : FVector::ZeroVector;

	// Save move parameters.
	const float DeltaTime = ServerData->GetServerMoveDeltaTime(TimeStamp) * CharacterOwner->CustomTimeDilation;

	ServerData->CurrentClientTimeStamp = TimeStamp;
	ServerData->ServerTimeStamp = GetWorld()->TimeSeconds;

	if (PC)
	{
		FRotator ViewRot;
		ViewRot.Pitch = ViewPitch;
		ViewRot.Yaw = ViewYaw;
		ViewRot.Roll = 0.f;
		PC->SetControlRotation(ViewRot);
	}
	if (!bServerReadyForClient)
	{
		return;
	}

	// Perform actual movement
	if ((CharacterOwner->GetWorldSettings()->Pauser == NULL) && (DeltaTime > 0.f))
	{
		if (PC)
		{
			PC->UpdateRotation(DeltaTime);
		}
		MoveAutonomous(TimeStamp, DeltaTime, MoveFlags, Accel);
	}
	UE_LOG(LogNetPlayerMovement, Verbose, TEXT("ServerMove Time %f Acceleration %s Position %s DeltaTime %f"),
		TimeStamp, *Accel.ToString(), *CharacterOwner->GetActorLocation().ToString(), DeltaTime);

	ServerMoveHandleClientError(TimeStamp, DeltaTime, Accel, ClientLoc, ClientMovementBase, ClientBaseBoneName, ClientMovementMode);
}

void UUTCharacterMovement::ProcessOldServerMove(float OldTimeStamp, FVector OldAccel, float OldYaw, uint8 OldMoveFlags)
{
	if (!HasValidData() || !IsComponentTickEnabled())
	{
		return;
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (!VerifyClientTimeStamp(OldTimeStamp, *ServerData))
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (PC)
	{
		FRotator ViewRot;
		ViewRot.Pitch = PC->GetControlRotation().Pitch;
		ViewRot.Yaw = OldYaw;
		ViewRot.Roll = 0.f;
		PC->SetControlRotation(ViewRot);
	}

	UE_LOG(LogNetPlayerMovement, Warning, TEXT("Recovered move from OldTimeStamp %f, DeltaTime: %f"), OldTimeStamp, OldTimeStamp - ServerData->CurrentClientTimeStamp);
	const float MaxResponseTime = ServerData->MaxResponseTime * CharacterOwner->GetWorldSettings()->GetEffectiveTimeDilation();

	MoveAutonomous(OldTimeStamp, FMath::Min(OldTimeStamp - ServerData->CurrentClientTimeStamp, MaxResponseTime), OldMoveFlags, OldAccel);
	ServerData->CurrentClientTimeStamp = OldTimeStamp;
}

void UUTCharacterMovement::ClientAckGoodMove_Implementation(float TimeStamp)
{
	if (!HasValidData() || !IsComponentTickEnabled())
	{
		return;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	check(ClientData);

	// Ack move if it has not expired.
	int32 MoveIndex = ClientData->GetSavedMoveIndex(TimeStamp);

	// @TODO FIXMESTEVE - need this implementation, because it's legit to sometimes have moves be already gone (after client adjustment called)
	if (MoveIndex == INDEX_NONE)
	{
		return;
	}
	ClientData->AckMove(MoveIndex);
}






