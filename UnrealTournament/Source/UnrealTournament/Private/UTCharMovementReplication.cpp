// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacterMovement.h"
#include "GameFramework/GameNetworkManager.h"

//======================================================
// Networking Support

float UUTCharacterMovement::GetCurrentMovementTime() const
{
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

void UUTCharacterMovement::ResetPredictionData_Client()
{
	Super::ResetPredictionData_Client();
	ResetTimers();
}

void UUTCharacterMovement::MoveAutonomous
(
float ClientTimeStamp,
float DeltaTime,
uint8 CompressedFlags,
const FVector& NewAccel
)
{
	if (!HasValidData())
	{
		return;
	}
	CurrentServerMoveTime = ClientTimeStamp;
	//UE_LOG(UT, Warning, TEXT("+++++++Set server move time %f"), CurrentServerMoveTime); //MinTimeBetweenTimeStampResets
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
		UE_LOG(UTNet, Warning, TEXT("%f SPRINTING changed from %d to %d"), ClientTimeStamp, bOldSprinting, bIsSprinting);
	}*/
	PerformMovement(DeltaTime);

	// If not playing root motion, tick animations after physics. We do this here to keep events, notifies, states and transitions in sync with client updates.
	if (!CharacterOwner->bClientUpdating && !CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh())
	{
		TickCharacterPose(DeltaTime);
		// TODO: SaveBaseLocation() in case tick moves us?
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
	bIsFloorSliding = (DodgeFlags == 6);
	bPressedSlide = (DodgeFlags == 7);
	bool bOldWillFloorSlide = bWantsFloorSlide;
	bWantsWallSlide = ((Flags & FSavedMove_Character::FLAG_Custom_0) != 0);
	bWantsFloorSlide = ((Flags & FSavedMove_Character::FLAG_Custom_1) != 0);
	bShotSpawned = ((Flags & FSavedMove_Character::FLAG_Custom_3) != 0);
	if (!bOldWillFloorSlide && bWantsFloorSlide)
	{
		FloorSlideTapTime = GetCurrentMovementTime();
	}
	if (Cast<AUTCharacter>(CharacterOwner))
	{
		Cast<AUTCharacter>(CharacterOwner)->bRepFloorSliding = bIsFloorSliding;
	}
}

void UUTCharacterMovement::ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode)
{
	//UE_LOG(UTNet, Warning, TEXT("Received client adjust position for %f"), TimeStamp);
	// use normal replication when simulating physics
	AUTCharacter* UTOwner = Cast<AUTCharacter>(CharacterOwner);
	if (UTOwner == NULL || UTOwner->GetRootComponent() == NULL || (!UTOwner->GetRootComponent()->IsSimulatingPhysics() && !UTOwner->IsRagdoll()))
	{
		Super::ClientAdjustPosition_Implementation(TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);
	}
	else
	{
		ClientAckGoodMove_Implementation(TimeStamp);
	}
}

void UUTCharacterMovement::SmoothClientPosition(float DeltaSeconds)
{
	if (!HasValidData() || GetNetMode() != NM_Client)
	{
		return;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (ClientData && ClientData->bSmoothNetUpdates && CharacterOwner->GetMesh() && !CharacterOwner->GetMesh()->IsSimulatingPhysics())
	{
		// faster interpolation if stopped
		float SmoothTime = Velocity.IsZero() ? 0.5f*ClientData->SmoothNetUpdateTime : ClientData->SmoothNetUpdateTime;

		// smooth interpolation of mesh translation to avoid popping of other client pawns, unless driving or ragdoll or low tick rate
		if (DeltaSeconds < SmoothTime)
		{
			ClientData->MeshTranslationOffset = (ClientData->MeshTranslationOffset * (1.f - DeltaSeconds / SmoothTime));
		}
		else
		{
			ClientData->MeshTranslationOffset = FVector::ZeroVector;
		}

		if (IsMovingOnGround())
		{
			// don't smooth Z position if walking on ground
			ClientData->MeshTranslationOffset.Z = 0.f;
		}

		const FVector NewRelTranslation = CharacterOwner->ActorToWorld().InverseTransformVectorNoScale(ClientData->MeshTranslationOffset + CharacterOwner->GetBaseTranslationOffset());
		CharacterOwner->GetMesh()->SetRelativeLocation(NewRelTranslation);
		//DrawDebugSphere(GetWorld(), CharacterOwner->GetActorLocation(), 30.f, 8, FColor::Yellow);
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
	const bool bRealWantsFloorSlide = bWantsFloorSlide;
	const bool bRealWantsWallSlide = bWantsWallSlide;

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
	bWantsFloorSlide = bRealWantsFloorSlide;
	bWantsWallSlide = bRealWantsWallSlide;

	return bResult;
}

void UUTCharacterMovement::SimulateMovement(float DeltaSeconds)
{
	if (!HasValidData() || UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	bool bWasFalling = (MovementMode == MOVE_Falling);
	FVector RealVelocity = Velocity; // Used now to keep our forced clientside decel from affecting animation

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
		SimulateMovement_Internal(DeltaTime);

		if (CharacterOwner->Role == ROLE_SimulatedProxy)
		{
			// update velocity with replicated acceleration - handle falling also
			// For now, pretend 0 accel if walking on ground
			if (MovementMode != MOVE_Falling)
			{
				LastCheckedAgainstWall = 0.f;
			}
			if (MovementMode == MOVE_Walking)
			{
				bIsAgainstWall = false;
				float Speed = Velocity.Size();
				if (bWasFalling && (Speed > MaxWalkSpeed))
				{
					if (Speed > SprintSpeed)
					{
						SimulatedVelocity = DodgeLandingSpeedFactor * Velocity.GetSafeNormal2D();
					}
					else
					{
						SimulatedVelocity = MaxWalkSpeed * Velocity.GetSafeNormal2D();
					}
				}
				else
				{
					bIsSprinting = (RealVelocity.SizeSquared() > 1.01f * FMath::Square(MaxWalkSpeed));

					const float MaxAccel = GetMaxAcceleration();
					float MaxSpeed = GetMaxSpeed();

					// Apply braking or deceleration
					const bool bZeroAcceleration = Acceleration.IsZero();
					const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

					// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
					if (bZeroAcceleration || bVelocityOverMax)
					{
						const FVector OldVelocity = Velocity;
						ApplyVelocityBraking(DeltaSeconds, GroundFriction, BrakingDecelerationWalking);

						// Don't allow braking to lower us below max speed if we started above it.
						if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
						{
							Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
						}
					}
					else if (!bZeroAcceleration)
					{
						// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
						const FVector AccelDir = Acceleration.GetSafeNormal();
						const float VelSize = Velocity.Size();
						Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaSeconds * GroundFriction, 1.f);
					}

					// Apply acceleration
					const float NewMaxSpeed = (IsExceedingMaxSpeed(MaxSpeed)) ? Velocity.Size() : MaxSpeed;
					Velocity += Acceleration * DeltaTime;
					if (Velocity.Size() > NewMaxSpeed)
					{
						Velocity = NewMaxSpeed * Velocity.GetSafeNormal();
					}
					SimulatedVelocity = Velocity;
					//UE_LOG(UT, Warning, TEXT("New simulated velocity %f %f"), Velocity.X, Velocity.Y);
				}
			}
			// @TODO FIXMESTEVE - need to update falling velocity after simulate also
		}
	}
	Velocity = RealVelocity;
}

void UUTCharacterMovement::SetReplicatedAcceleration(FRotator MovementRotation, uint8 CompressedAccel)
{
	MovementRotation.Pitch = 0.f;
	FVector CurrentDir = MovementRotation.Vector();
	FVector SideDir = (CurrentDir ^ FVector(0.f, 0.f, 1.f)).GetSafeNormal();

	FVector AccelDir(0.f);
	if (CompressedAccel & 1)
	{
		AccelDir += CurrentDir;
	}
	else if (CompressedAccel & 2)
	{
		AccelDir -= CurrentDir;
	}
	if (CompressedAccel & 4)
	{
		AccelDir += SideDir;
	}
	else if (CompressedAccel & 8)
	{
		AccelDir -= SideDir;
	}
	bIsSprinting = ((MovementMode == MOVE_Walking) && (Velocity.SizeSquared() > FMath::Square<float>(MaxWalkSpeed)));
	Acceleration = GetMaxAcceleration() * AccelDir.GetSafeNormal();
	//UE_LOG(UT, Warning, TEXT("New replcated velocity %f %f acceleration %f %f"), Velocity.X, Velocity.Y, Acceleration.X, Acceleration.Y);
}

// Waiting on update of UCharacterMovementComponent::CharacterMovement(), overriding for now
void UUTCharacterMovement::SimulateMovement_Internal(float DeltaSeconds)
{
	if (!HasValidData() || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	const bool bIsSimulatedProxy = (CharacterOwner->Role == ROLE_SimulatedProxy);

	// Workaround for replication not being updated initially
	if (bIsSimulatedProxy &&
		CharacterOwner->ReplicatedMovement.Location.IsZero() &&
		CharacterOwner->ReplicatedMovement.Rotation.IsZero() &&
		CharacterOwner->ReplicatedMovement.LinearVelocity.IsZero())
	{
		return;
	}

	// If base is not resolved on the client, we should not try to simulate at all
	if (CharacterOwner->GetReplicatedBasedMovement().IsBaseUnresolved())
	{
		UE_LOG(LogNetPlayerMovement, Verbose, TEXT("Base for simulated character '%s' is not resolved on client, skipping SimulateMovement"), *CharacterOwner->GetName());
		return;
	}

	FVector OldVelocity;
	FVector OldLocation;

	// Scoped updates can improve performance of multiple MoveComponent calls.
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

		if (bIsSimulatedProxy)
		{
			// Handle network changes
			if (bNetworkUpdateReceived)
			{
				bNetworkUpdateReceived = false;
				if (bNetworkMovementModeChanged)
				{
					bNetworkMovementModeChanged = false;
					ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());
				}
				else if (bJustTeleported)
				{
					// Make sure floor is current. We will continue using the replicated base, if there was one.
					bJustTeleported = false;
					UpdateFloorFromAdjustment();
				}
			}

			HandlePendingLaunch();
		}

		if (MovementMode == MOVE_None)
		{
			return;
		}

		AnalogInputModifier = 1.0f;				// Not currently used for simulated movement

		MaybeUpdateBasedMovement(DeltaSeconds);

		//DrawDebugLine(GetWorld(), CharacterOwner->GetActorLocation(), CharacterOwner->GetActorLocation() + 0.2f*Acceleration, FLinearColor::Green);

		// simulated pawns predict location
		OldVelocity = Velocity;
		OldLocation = UpdatedComponent->GetComponentLocation();
		FStepDownResult StepDownResult;
		MoveSmooth(Velocity, DeltaSeconds, &StepDownResult);

		// consume path following requested velocity
		bHasRequestedVelocity = false;

		// if simulated gravity, find floor and check if falling
		const bool bEnableFloorCheck = (!CharacterOwner->bSimGravityDisabled || !bIsSimulatedProxy);
		if (bEnableFloorCheck && (MovementMode == MOVE_Walking || MovementMode == MOVE_Falling))
		{
			const FVector CollisionCenter = UpdatedComponent->GetComponentLocation();
			if (StepDownResult.bComputedFloor)
			{
				CurrentFloor = StepDownResult.FloorResult;
			}
			else if (Velocity.Z <= 0.f)
			{
				FindFloor(CollisionCenter, CurrentFloor, Velocity.IsZero(), NULL);
			}
			else
			{
				CurrentFloor.Clear();
			}

			if (!CurrentFloor.IsWalkableFloor())
			{
				if ((Velocity.Z != 0.f) || (MovementMode == MOVE_Falling))
				{
					// No floor, must fall.
					Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaSeconds);
					SetMovementMode(MOVE_Falling);
				}
			}
			else
			{
				// Walkable floor
				if (MovementMode == MOVE_Walking)
				{
					AdjustFloorHeight();
					SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
				}
				else if (MovementMode == MOVE_Falling)
				{
					if (CurrentFloor.FloorDist <= MIN_FLOOR_DIST)
					{
						// Landed
						SetMovementMode(MOVE_Walking);
					}
					else
					{
						// Continue falling.
						Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaSeconds);
						CurrentFloor.Clear();
					}
				}
			}
		}

		OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	} // End scoped movement update

	// Call custom post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);

	SaveBaseLocation();
	UpdateComponentVelocity();
	bJustTeleported = false;

	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
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
		LastCheckedAgainstWall = GetWorld()->GetTimeSeconds();
		bIsAgainstWall = true;
		WallSlideNormal = Hit.Normal;
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, false);
	}
	else if (GetWorld()->GetTimeSeconds() - LastCheckedAgainstWall > 0.07f)
	{
		LastCheckedAgainstWall = GetWorld()->GetTimeSeconds();
		static const FName FallingTraceParamsTag = FName(TEXT("PhysFalling"));
		const float TestWalkTime = FMath::Max(DeltaSeconds, 0.05f);
		const FVector TestWalk = (FVector(0.f, 0.f, GetGravityZ()) * TestWalkTime + Velocity) * TestWalkTime;
		FCollisionQueryParams CapsuleQuery(FallingTraceParamsTag, false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(CapsuleQuery, ResponseParam);
		const FVector PawnLocation = CharacterOwner->GetActorLocation();
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
		const bool bHit = GetWorld()->SweepSingleByChannel(Hit, PawnLocation, PawnLocation + TestWalk, FQuat::Identity, CollisionChannel, GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleQuery, ResponseParam);
		bIsAgainstWall = bHit;
		if (bHit)
		{
			WallSlideNormal = Hit.Normal;
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

	if (ServerData->PendingAdjustment.bAckGoodMove)
	{
		if (GetWorld()->GetTimeSeconds() - LastGoodMoveAckTime > MinTimeBetweenClientAdjustments)
		{
			ClientAckGoodMove(ServerData->PendingAdjustment.TimeStamp);
			LastGoodMoveAckTime = GetWorld()->GetTimeSeconds();
		}
	}
	else if (GetWorld()->GetTimeSeconds() - LastClientAdjustmentTime > MinTimeBetweenClientAdjustments)
	{
		// in case of packet loss, more frequent correction updates if error is larger
		LastClientAdjustmentTime = bLargeCorrection ? GetWorld()->GetTimeSeconds() - 0.05f : GetWorld()->GetTimeSeconds();
		bool bHasBase = (ServerData->PendingAdjustment.NewBase != NULL) || (ServerData->PendingAdjustment.MovementMode == MOVE_Walking);
		if (CharacterOwner->IsPlayingNetworkedRootMotionMontage())
		{
			FRotator Rotation = ServerData->PendingAdjustment.NewRot.GetNormalized();
			FVector_NetQuantizeNormal CompressedRotation(Rotation.Pitch / 180.f, Rotation.Yaw / 180.f, Rotation.Roll / 180.f);
			//UE_LOG(UTNet, Warning, TEXT("SEND ClientAdjustRootMotionPosition"));
			ClientAdjustRootMotionPosition
				(
				ServerData->PendingAdjustment.TimeStamp,
				CharacterOwner->GetRootMotionAnimMontageInstance()->GetPosition(),
				ServerData->PendingAdjustment.NewLoc,
				CompressedRotation,
				ServerData->PendingAdjustment.NewVel.Z,
				ServerData->PendingAdjustment.NewBase,
				ServerData->PendingAdjustment.NewBaseBoneName,
				bHasBase,
				ServerData->PendingAdjustment.bBaseRelativePosition,
				ServerData->PendingAdjustment.MovementMode
				);
		}
		else if (!ServerData->PendingAdjustment.bBaseRelativePosition && (ServerData->PendingAdjustment.NewBaseBoneName == NAME_None))
		{
			//UE_LOG(UTNet, Warning, TEXT("SEND ClientNoBaseAdjustPosition"));
			ClientNoBaseAdjustPosition
				(
				ServerData->PendingAdjustment.TimeStamp,
				ServerData->PendingAdjustment.NewLoc,
				ServerData->PendingAdjustment.NewVel,
				ServerData->PendingAdjustment.MovementMode
				);
		}
		else
		{
			//UE_LOG(UTNet, Warning, TEXT("SEND ClientAdjustPosition"));
			ClientAdjustPosition
				(
				ServerData->PendingAdjustment.TimeStamp,
				ServerData->PendingAdjustment.NewLoc,
				ServerData->PendingAdjustment.NewVel,
				ServerData->PendingAdjustment.NewBase,
				ServerData->PendingAdjustment.NewBaseBoneName,
				bHasBase,
				ServerData->PendingAdjustment.bBaseRelativePosition,
				ServerData->PendingAdjustment.MovementMode
				);
		}
	}

	ServerData->PendingAdjustment.TimeStamp = 0;
	ServerData->PendingAdjustment.bAckGoodMove = false;
}

void UUTCharacterMovement::ClientNoBaseAdjustPosition_Implementation(float TimeStamp, FVector NewLocation, FVector NewVelocity, uint8 ServerMovementMode)
{
	if (!HasValidData() || !IsComponentTickEnabled())
	{
		return;
	}
	if (MovementBaseUtility::UseRelativeLocation(CharacterOwner->GetMovementBase()))
	{
		// need to change to no base, cause server doesn't think I'm based on something relative.
		SetBase(NULL);
	}
	ClientAdjustPosition_Implementation(TimeStamp, NewLocation, NewVelocity, CharacterOwner->GetMovementBase(), NAME_None, (CharacterOwner->GetMovementBase() != NULL), false, ServerMovementMode);
}

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

	// Get a SavedMove object to store the movement in.
	FSavedMovePtr NewMove = ClientData->CreateSavedMove();
	if (NewMove.IsValid() == false)
	{
		return;
	}

	NewMove->SetMoveFor(CharacterOwner, DeltaTime, NewAcceleration, *ClientData);
	NewMove->SetInitialPosition(CharacterOwner);

	// Perform the move locally
	Acceleration = ScaleInputAcceleration(NewMove->Acceleration);
	CharacterOwner->ClientRootMotionParams.Clear();
	PerformMovement(NewMove->DeltaTime);

	AUTCharacter* UTCharacterOwner = Cast<AUTCharacter>(CharacterOwner);
	bShotSpawned = (UTCharacterOwner && UTCharacterOwner->GetWeapon()) ? UTCharacterOwner->GetWeapon()->WillSpawnShot(DeltaTime) : false;
	NewMove->PostUpdate(CharacterOwner, FSavedMove_Character::PostUpdate_Record);

	// Add NewMove to the list
	ClientData->SavedMoves.Push(NewMove);

	UTCallServerMove();
}

bool UUTCharacterMovement::CanDelaySendingMove(const FSavedMovePtr& NewMove)
{
	// don't delay if just spawned shot or dodged 
	return !NewMove.IsValid() || !((FSavedMove_UTCharacter*)(NewMove.Get()))->NeedsRotationSent();
}

bool FSavedMove_UTCharacter::NeedsRotationSent() const
{
	return (bPressedDodgeForward || bPressedDodgeBack || bPressedDodgeLeft || bPressedDodgeRight || bPressedSlide || bShotSpawned);
}

bool FSavedMove_UTCharacter::IsImportantMove(const FSavedMovePtr& ComparedMove) const
{
	if (bPressedDodgeForward || bPressedDodgeBack || bPressedDodgeLeft || bPressedDodgeRight || bPressedSlide || bShotSpawned)
	{
		return true;
	}

	if (!ComparedMove.IsValid())
	{
		// if no previous move to compare, always send impulses
		return bPressedJump;
	}

	// Check if any important movement flags have changed status.
	if ((bPressedJump && (bPressedJump != ComparedMove->bPressedJump)) || (bWantsToCrouch != ComparedMove->bWantsToCrouch))
	{
		return true;
	}

	if (MovementMode != ComparedMove->MovementMode)
	{
		return true;
	}

	// check if acceleration has changed significantly
	if (Acceleration != ComparedMove->Acceleration)
	{
		// Compare magnitude and orientation
		if ((FMath::Abs(AccelMag - ComparedMove->AccelMag) > AccelMagThreshold) || ((AccelNormal | ComparedMove->AccelNormal) < AccelDotThreshold))
		{
			return true;
		}
	}
	return false;
}

void UUTCharacterMovement::UTCallServerMove()
{
	AUTCharacter* UTCharacterOwner = Cast<AUTCharacter>(CharacterOwner);
	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (!UTCharacterOwner || !ClientData || (ClientData->SavedMoves.Num() == 0))
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());

	// Decide whether to hold off on move
	// never delay if spawning shot must sync
	const FSavedMovePtr& NewMove = ClientData->SavedMoves.Last();
	if (CanDelaySendingMove(NewMove))
	{
		UPlayer* Player = (PC ? PC->Player : NULL);
		int32 CurrentNetSpeed = Player ? Player->CurrentNetSpeed : 0;
		// @TODO FIXMESTEVE - base acceptable netmovedelta on CurrentNetSpeed
		float NetMoveDelta = NewMove->IsImportantMove(ClientData->LastAckedMove) ? 0.017f : 0.033f;

		if (((NewMove->TimeStamp - ClientData->ClientUpdateTime) * CharacterOwner->GetWorldSettings()->GetEffectiveTimeDilation() < NetMoveDelta))
		{
			//UE_LOG(UT, Warning, TEXT("Delay sending %f flags %d netspeed %d"), NewMove->TimeStamp, NewMove->GetCompressedFlags(), CurrentNetSpeed);
			return;
		}
	}

/*	UE_LOG(LogNetPlayerMovement, Warning, TEXT("Client ReplicateMove Time %f Acceleration %s Position %s flags %d movementmode %d"),
		NewMove->TimeStamp, *NewMove->Acceleration.ToString(), *CharacterOwner->GetActorLocation().ToString(), NewMove->GetCompressedFlags(), int32(NewMove->MovementMode));
*/
	// Find the oldest unacknowledged sent important move (OldMove).
	// Don't include the last move because it may be combined with the next new move.
	// A saved move is interesting if it differs significantly from the last acknowledged move
	FSavedMovePtr OldMovePtr = NULL;
	if (ClientData->LastAckedMove.IsValid())
	{
		for (int32 i = 0; i < ClientData->SavedMoves.Num() - 1; i++)
		{
			const FSavedMovePtr& CurrentMove = ClientData->SavedMoves[i];
			if (CurrentMove->TimeStamp > ClientData->ClientUpdateTime)
			{
				// move hasn't been sent yet
				break;
			}
			else if (CurrentMove->IsImportantMove(ClientData->LastAckedMove))
			{
				OldMovePtr = CurrentMove;
			}
		}
	}

	// send old move if it exists
	if (OldMovePtr.IsValid())
	{
		const FSavedMove_Character* OldMove = OldMovePtr.Get();
		//UE_LOG(UTNet, Warning, TEXT("Sending old move %f"), OldMove->TimeStamp);
		UTCharacterOwner->UTServerMoveOld(OldMove->TimeStamp, OldMove->Acceleration, OldMove->SavedControlRotation.Yaw, OldMove->GetCompressedFlags());
	}

	for (int32 i = 0; i<ClientData->SavedMoves.Num()-1; i++)
	{
		const FSavedMovePtr& MoveToSend = ClientData->SavedMoves[i];
		if (MoveToSend.IsValid() && (MoveToSend->TimeStamp > ClientData->ClientUpdateTime))
		{
			//UE_LOG(UTNet, Warning, TEXT("Sending pending %f  flags %d"), MoveToSend->TimeStamp, MoveToSend->GetCompressedFlags());
			ClientData->ClientUpdateTime = MoveToSend->TimeStamp;
			if (((FSavedMove_UTCharacter*)(MoveToSend.Get()))->NeedsRotationSent())
			{
				UTCharacterOwner->UTServerMoveSaved(MoveToSend->TimeStamp, MoveToSend->Acceleration, MoveToSend->GetCompressedFlags(), MoveToSend->SavedControlRotation.Yaw, MoveToSend->SavedControlRotation.Pitch);
			}
			else
			{
				UTCharacterOwner->UTServerMoveQuick(MoveToSend->TimeStamp, MoveToSend->Acceleration, MoveToSend->GetCompressedFlags());
			}
		}
	}

	if (NewMove.IsValid() && (NewMove->TimeStamp > ClientData->ClientUpdateTime))
	{
		// Determine if we send absolute or relative location
		UPrimitiveComponent* ClientMovementBase = NewMove->EndBase.Get();
		bool bUseRelativeLocation = MovementBaseUtility::UseRelativeLocation(ClientMovementBase);
		const FVector SendLocation = bUseRelativeLocation ? NewMove->SavedRelativeLocation : NewMove->SavedLocation;
		if (!bUseRelativeLocation)
		{
			// location isn't relative, don't need to replicate base
			ClientMovementBase = NULL;
			NewMove->EndBoneName = NAME_None;
		}
		//UE_LOG(UTNet, Warning, TEXT("Sending current move %f relative %d flags %d  dodge %d sprint %d shotspawned %d"), NewMove->TimeStamp, bUseRelativeLocation, NewMove->GetCompressedFlags(), (((const FSavedMove_UTCharacter*)NewMove.Get())->bPressedDodgeForward || ((const FSavedMove_UTCharacter*)NewMove.Get())->bPressedDodgeForward || ((const FSavedMove_UTCharacter*)NewMove.Get())->bPressedDodgeForward), ((const FSavedMove_UTCharacter*)NewMove.Get())->bSavedIsSprinting, ((const FSavedMove_UTCharacter*)NewMove.Get())->bShotSpawned);
		ClientData->ClientUpdateTime = NewMove->TimeStamp;
		UTCharacterOwner->UTServerMove
			(
			NewMove->TimeStamp,
			NewMove->Acceleration,
			SendLocation,
			NewMove->GetCompressedFlags(),
			NewMove->SavedControlRotation.Yaw,
			NewMove->SavedControlRotation.Pitch,
			ClientMovementBase,
			NewMove->EndBoneName,
			NewMove->MovementMode
			);
	}

	APlayerCameraManager* PlayerCameraManager = (PC ? PC->PlayerCameraManager : NULL);
	if (PlayerCameraManager != NULL && PlayerCameraManager->bUseClientSideCameraUpdates)
	{
		UE_LOG(UT, Warning, TEXT("WTF WTF WTF WTF!!!!!!!!!!!!!!!!"));
		PlayerCameraManager->bShouldSendClientSideCameraUpdate = true;
	}
}

void UUTCharacterMovement::ProcessServerMove(float TimeStamp, FVector InAccel, FVector ClientLoc, uint8 MoveFlags, float ViewYaw, float ViewPitch, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	if (!HasValidData() || !IsComponentTickEnabled())
	{
		//UE_LOG(UT, Warning, TEXT("Out of ProcessServerMove 1"));
		return;
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (!UTVerifyClientTimeStamp(TimeStamp, *ServerData))
	{
		//UE_LOG(UTNet, Warning, TEXT("Failed UTVerifyClientTimeStamp"));
		return;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	bool bServerReadyForClient = PC ? PC->NotifyServerReceivedClientData(CharacterOwner, TimeStamp) : true;
	const FVector Accel = bServerReadyForClient ? InAccel : FVector::ZeroVector;

	// Save move parameters.
	const float DeltaTime = ServerData->GetServerMoveDeltaTime(TimeStamp) * CharacterOwner->CustomTimeDilation;
	ServerData->CurrentClientTimeStamp = TimeStamp;
	ServerData->ServerTimeStamp = GetWorld()->GetTimeSeconds();
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
		//UE_LOG(UT, Warning, TEXT("Failed bServerReadyForClient"));
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

		FVector CurrLoc = CharacterOwner->GetActorLocation();
		if (InAccel.IsZero() && (MovementMode == MOVE_Walking) && (ClientLoc.X == CurrLoc.X) && (ClientLoc.Y == CurrLoc.Y) && (FMath::Abs(CurrLoc.Z - ClientLoc.Z) < 0.1f))
		{
			CharacterOwner->SetActorLocation(ClientLoc);
		}
	}
	UE_LOG(LogNetPlayerMovement, Verbose, TEXT("ServerMove Time %f Acceleration %s Position %s DeltaTime %f"),
		TimeStamp, *Accel.ToString(), *CharacterOwner->GetActorLocation().ToString(), DeltaTime);

	UTServerMoveHandleClientError(TimeStamp, DeltaTime, Accel, ClientLoc, ClientMovementBase, ClientBaseBoneName, ClientMovementMode);
}

void UUTCharacterMovement::UTServerMoveHandleClientError(float TimeStamp, float DeltaTime, const FVector& Accel, const FVector& RelativeClientLoc, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	// Don't prevent more recent updates from being sent if received this frame.
	// We're going to send out an update anyway, might as well be the most recent one.
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if ((ServerData->LastUpdateTime != GetWorld()->TimeSeconds) && GetDefault<AGameNetworkManager>()->WithinUpdateDelayBounds(PC, ServerData->LastUpdateTime))
	{
		return;
	}

	// Offset may be relative to base component
	FVector ClientLoc = RelativeClientLoc;
	if (MovementBaseUtility::UseRelativeLocation(ClientMovementBase))
	{
		FVector BaseLocation;
		FQuat BaseRotation;
		MovementBaseUtility::GetMovementBaseTransform(ClientMovementBase, ClientBaseBoneName, BaseLocation, BaseRotation);
		ClientLoc += BaseLocation;
	}

	// Compute the client error from the server's position
	const FVector LocDiff = CharacterOwner->GetActorLocation() - ClientLoc;
	const uint8 CurrentPackedMovementMode = PackNetworkMovementMode();
	bool bMovementModeDiffers = (CurrentPackedMovementMode != ClientMovementMode);
	if (bMovementModeDiffers)
	{
		// don't differentiate between falling and walking for this check
		// @TODO FIXMESTEVE maybe make sure the transition isn't more than 1 frame off?
		if (CurrentPackedMovementMode == 2)
		{
			bMovementModeDiffers = (ClientMovementMode != 1);
		}
		else if (CurrentPackedMovementMode == 1)
		{
			bMovementModeDiffers = (ClientMovementMode != 2);
		}
	}

	// If client has accumulated a noticeable positional error, correct him.
	if (ExceedsAllowablePositionError(LocDiff) || bMovementModeDiffers)
	{
		UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
		ServerData->PendingAdjustment.NewVel = Velocity;
		ServerData->PendingAdjustment.NewRot = CharacterOwner->GetActorRotation();

		ServerData->PendingAdjustment.bBaseRelativePosition = MovementBaseUtility::UseRelativeLocation(MovementBase);
		if (ServerData->PendingAdjustment.bBaseRelativePosition)
		{
			// Relative location
			ServerData->PendingAdjustment.NewLoc = CharacterOwner->GetBasedMovement().Location;
			ServerData->PendingAdjustment.NewBase = MovementBase;
			ServerData->PendingAdjustment.NewBaseBoneName = CharacterOwner->GetBasedMovement().BoneName;

			// TODO: this could be a relative rotation, but all client corrections ignore rotation right now except the root motion one, which would need to be updated.
			//ServerData->PendingAdjustment.NewRot = CharacterOwner->GetBasedMovement().Rotation;
		}
		else
		{
			ServerData->PendingAdjustment.NewLoc = CharacterOwner->GetActorLocation();
			ServerData->PendingAdjustment.NewBase = NULL;
			ServerData->PendingAdjustment.NewBaseBoneName = NAME_None;
		}

		// @TODO FIXMESTEVE configurable property controlled
		bLargeCorrection = (LocDiff.Size() > LargeCorrectionThreshold);

/*		if (bMovementModeDiffers)
		{
			UE_LOG(UTNet, Warning, TEXT("******** MOVEMENTMODE Client Error at %f is %f Accel %s LocDiff %s ClientLoc %s, ServerLoc: %s, MovementMode %d vs Client %d actual %d"),
				TimeStamp, LocDiff.Size(), *Accel.ToString(), *LocDiff.ToString(), *ClientLoc.ToString(), *CharacterOwner->GetActorLocation().ToString(), int32(CurrentPackedMovementMode), int32(ClientMovementMode), int32(MovementMode));
		}
		else
		{
			UE_LOG(UTNet, Warning, TEXT("******** UTClient Error at %f is %f Accel %s LocDiff %s ClientLoc %s, ServerLoc: %s"),
				TimeStamp, LocDiff.Size(), *Accel.ToString(), *LocDiff.ToString(), *ClientLoc.ToString(), *CharacterOwner->GetActorLocation().ToString(), *GetNameSafe(MovementBase));
		}
*/
		ServerData->LastUpdateTime = GetWorld()->TimeSeconds;
		ServerData->PendingAdjustment.DeltaTime = DeltaTime;
		ServerData->PendingAdjustment.TimeStamp = TimeStamp;
		ServerData->PendingAdjustment.bAckGoodMove = false;
		ServerData->PendingAdjustment.MovementMode = CurrentPackedMovementMode;
	}
	else
	{
		/*if (!LocDiff.IsZero())
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("+++++++++++++++++++++++ SMALL Error at %f is %f Accel %s LocDiff %s ClientLoc %s, ServerLoc: %s, MovementMode %d vs Client %d"),
				TimeStamp, LocDiff.Size(), *Accel.ToString(), *LocDiff.ToString(), *ClientLoc.ToString(), *CharacterOwner->GetActorLocation().ToString(), CurrentPackedMovementMode, ClientMovementMode);
		}*/
		// acknowledge receipt of this successful servermove()
		ServerData->PendingAdjustment.TimeStamp = TimeStamp;
		ServerData->PendingAdjustment.bAckGoodMove = true;
	}
}

void UUTCharacterMovement::NeedsClientAdjustment()
{
	LastClientAdjustmentTime = -1.f;
}

bool UUTCharacterMovement::ExceedsAllowablePositionError(FVector LocDiff) const
{
	return (LocDiff | LocDiff) > MaxPositionErrorSquared;
}


void UUTCharacterMovement::ProcessQuickServerMove(float TimeStamp, FVector InAccel, uint8 MoveFlags)
{
	if (!HasValidData() || !IsComponentTickEnabled())
	{
		return;
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (!UTVerifyClientTimeStamp(TimeStamp, *ServerData))
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	bool bServerReadyForClient = PC ? PC->NotifyServerReceivedClientData(CharacterOwner, TimeStamp) : true;
	const FVector Accel = bServerReadyForClient ? InAccel : FVector::ZeroVector;

	// Save move parameters.
	const float DeltaTime = ServerData->GetServerMoveDeltaTime(TimeStamp) * CharacterOwner->CustomTimeDilation;
	ServerData->CurrentClientTimeStamp = TimeStamp;
	ServerData->ServerTimeStamp = GetWorld()->GetTimeSeconds();
	//UE_LOG(UT, Warning, TEXT("2ServerTimeStamp to %f"), ServerData->ServerTimeStamp);

	if (!bServerReadyForClient)
	{
		return;
	}

	// Perform actual movement
	if ((CharacterOwner->GetWorldSettings()->Pauser == NULL) && (DeltaTime > 0.f))
	{
		MoveAutonomous(TimeStamp, DeltaTime, MoveFlags, Accel);
	}
	UE_LOG(LogNetPlayerMovement, Verbose, TEXT("QuickServerMove Time %f Acceleration %s Position %s DeltaTime %f"),
		TimeStamp, *Accel.ToString(), *CharacterOwner->GetActorLocation().ToString(), DeltaTime);
}

void UUTCharacterMovement::ProcessSavedServerMove(float TimeStamp, FVector InAccel, uint8 MoveFlags, float ViewYaw, float ViewPitch)
{
	if (!HasValidData() || !IsComponentTickEnabled())
	{
		return;
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (!UTVerifyClientTimeStamp(TimeStamp, *ServerData))
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	bool bServerReadyForClient = PC ? PC->NotifyServerReceivedClientData(CharacterOwner, TimeStamp) : true;
	const FVector Accel = bServerReadyForClient ? InAccel : FVector::ZeroVector;

	// Save move parameters.
	const float DeltaTime = ServerData->GetServerMoveDeltaTime(TimeStamp) * CharacterOwner->CustomTimeDilation;
	ServerData->CurrentClientTimeStamp = TimeStamp;
	ServerData->ServerTimeStamp = GetWorld()->GetTimeSeconds();
	//UE_LOG(UT, Warning, TEXT("3ServerTimeStamp to %f"), ServerData->ServerTimeStamp);

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
	UE_LOG(LogNetPlayerMovement, Verbose, TEXT("SavedServerMove Time %f Acceleration %s Position %s DeltaTime %f"),
		TimeStamp, *Accel.ToString(), *CharacterOwner->GetActorLocation().ToString(), DeltaTime);
}

void UUTCharacterMovement::ProcessOldServerMove(float OldTimeStamp, FVector OldAccel, float OldYaw, uint8 OldMoveFlags)
{
	if (!HasValidData() || !IsComponentTickEnabled())
	{
		return;
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (!UTVerifyClientTimeStamp(OldTimeStamp, *ServerData))
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

	//UE_LOG(LogNetPlayerMovement, Warning, TEXT("Recovered move from OldTimeStamp %f, DeltaTime: %f"), OldTimeStamp, OldTimeStamp - ServerData->CurrentClientTimeStamp);
	const float MaxResponseTime = ServerData->MaxResponseTime * CharacterOwner->GetWorldSettings()->GetEffectiveTimeDilation();

	MoveAutonomous(OldTimeStamp, FMath::Min(OldTimeStamp - ServerData->CurrentClientTimeStamp, MaxResponseTime), OldMoveFlags, OldAccel);
	ServerData->CurrentClientTimeStamp = OldTimeStamp;
}

void UUTCharacterMovement::ClientAckGoodMove_Implementation(float TimeStamp)
{
	if (!HasValidData())
	{
		return;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	check(ClientData);
	//UE_LOG(UT, Warning, TEXT("Ack ping is %f vs ExactPing %f"), GetCurrentMovementTime() - TimeStamp, CharacterOwner->PlayerState->ExactPing); 

	AUTPlayerState* UTPS = Cast<AUTPlayerState>(CharacterOwner->PlayerState);
	if (UTPS)
	{
		UTPS->CalculatePing(GetCurrentMovementTime() - TimeStamp);
	}

	// Ack move if it has not expired.
	int32 MoveIndex = ClientData->GetSavedMoveIndex(TimeStamp);

	// It's legit to sometimes have moves be already gone (after client adjustment called)
	if (MoveIndex == INDEX_NONE)
	{
		return;
	}
	ClientData->AckMove(MoveIndex);
}

bool UUTCharacterMovement::UTVerifyClientTimeStamp(float TimeStamp, FNetworkPredictionData_Server_Character & ServerData)
{
	// Very large deltas happen around a TimeStamp reset.
	const float DeltaTimeStamp = (TimeStamp - ServerData.CurrentClientTimeStamp);
	if (FMath::Abs(DeltaTimeStamp) > (MinTimeBetweenTimeStampResets * 0.5f))
	{
		// Client is resetting TimeStamp to increase accuracy.
		if (DeltaTimeStamp < 0.f)
		{
			//UE_LOG(UTNet, Log, TEXT("TimeStamp reset detected. CurrentTimeStamp: %f, new TimeStamp: %f"), ServerData.CurrentClientTimeStamp, TimeStamp);
			ServerData.CurrentClientTimeStamp = 0.f;
			AdjustMovementTimers(-1.f*DeltaTimeStamp);
			return true;
		}
		else
		{
			// We already reset the TimeStamp, but we just got an old outdated move before the switch.
			// Just ignore it.
			//UE_LOG(UTNet, Log, TEXT("TimeStamp expired. Before TimeStamp Reset. CurrentTimeStamp: %f, TimeStamp: %f"), ServerData.CurrentClientTimeStamp, TimeStamp);
			return false;
		}
	}

	// If TimeStamp is in the past, move is outdated, ignore it.
	if (TimeStamp <= ServerData.CurrentClientTimeStamp)
	{
		//UE_LOG(LogNetPlayerMovement, Log, TEXT("TimeStamp expired. %f, CurrentTimeStamp: %f"), TimeStamp, ServerData.CurrentClientTimeStamp);
		return false;
	}

	//UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("TimeStamp %f Accepted! CurrentTimeStamp: %f"), TimeStamp, ServerData.CurrentClientTimeStamp);
	return true;
}

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
	if (bSavedWantsWallSlide != ((FSavedMove_UTCharacter*)&NewMove)->bSavedWantsWallSlide)
	{
		return false;
	}
	if (bSavedWantsSlide != ((FSavedMove_UTCharacter*)&NewMove)->bSavedWantsSlide)
	{
		return false;
	}

	bool bPressedDodge = bPressedDodgeForward || bPressedDodgeBack || bPressedDodgeLeft || bPressedDodgeRight || bPressedSlide;
	bool bNewPressedDodge = ((FSavedMove_UTCharacter*)&NewMove)->bPressedDodgeForward || ((FSavedMove_UTCharacter*)&NewMove)->bPressedDodgeBack || ((FSavedMove_UTCharacter*)&NewMove)->bPressedDodgeLeft || ((FSavedMove_UTCharacter*)&NewMove)->bPressedDodgeRight || ((FSavedMove_UTCharacter*)&NewMove)->bPressedSlide;
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
	else if (bPressedSlide)
	{
		Result |= (7 << 2);
	}
	else if (bSavedIsSprinting)
	{
		Result |= (5 << 2);
	}
	else if (bSavedIsRolling)
	{
		Result |= (6 << 2);
	}
	if (bSavedWantsWallSlide)
	{
		Result |= FLAG_Custom_0;
	}
	if (bSavedWantsSlide)
	{
		Result |= FLAG_Custom_1;
	}

	if (bSavedIsEmoting)
	{
		Result |= FLAG_Custom_2;
	}

	if (bShotSpawned)
	{
		Result |= FLAG_Custom_3;
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
	bSavedWantsWallSlide = false;
	bSavedWantsSlide = false;
	bSavedIsEmoting = false;
	SavedMultiJumpCount = 0;
	SavedWallDodgeCount = 0;
	SavedSprintStartTime = 0.f;
	SavedDodgeResetTime = 0.f;
	SavedFloorSlideEndTime = 0.f;
	bSavedJumpAssisted = false;
	bSavedIsDodging = false;
	bPressedSlide = false;
	bShotSpawned = false;
}

void FSavedMove_UTCharacter::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
	UUTCharacterMovement* UTCharMov = Cast<UUTCharacterMovement>(Character->GetCharacterMovement());
	if (UTCharMov)
	{
		bPressedDodgeForward = UTCharMov->bPressedDodgeForward;
		bPressedDodgeBack = UTCharMov->bPressedDodgeBack;
		bPressedDodgeLeft = UTCharMov->bPressedDodgeLeft;
		bPressedDodgeRight = UTCharMov->bPressedDodgeRight;
		bSavedIsSprinting = UTCharMov->bIsSprinting;
		bSavedIsRolling = UTCharMov->bIsFloorSliding;
		bSavedWantsWallSlide = UTCharMov->WantsWallSlide();
		bSavedWantsSlide = UTCharMov->WantsFloorSlide();
		bSavedIsEmoting = UTCharMov->bIsEmoting;
		SavedMultiJumpCount = UTCharMov->CurrentMultiJumpCount;
		SavedWallDodgeCount = UTCharMov->CurrentWallDodgeCount;
		SavedSprintStartTime = UTCharMov->SprintStartTime;
		SavedDodgeResetTime = UTCharMov->DodgeResetTime;
		SavedFloorSlideEndTime = UTCharMov->FloorSlideEndTime;
		bSavedJumpAssisted = UTCharMov->bJumpAssisted;
		bSavedIsDodging = UTCharMov->bIsDodging;
		bPressedSlide = UTCharMov->bPressedSlide;
		//UE_LOG(UTNet, Warning, TEXT("set move %f Dodge %d Sprint %d saved sprint start %f"), TimeStamp, (bPressedDodgeForward || bPressedDodgeBack || bPressedDodgeLeft || bPressedDodgeRight), bSavedIsSprinting, SavedSprintStartTime);
	}

	// Round acceleration, so sent version and locally used version always match
	Acceleration.X = FMath::RoundToFloat(Acceleration.X);
	Acceleration.Y = FMath::RoundToFloat(Acceleration.Y);
	Acceleration.Z = FMath::RoundToFloat(Acceleration.Z);
}

void FSavedMove_UTCharacter::PrepMoveFor(ACharacter* Character)
{
	Super::PrepMoveFor(Character);

	UUTCharacterMovement* UTCharMov = Cast<UUTCharacterMovement>(Character->GetCharacterMovement());
	if (UTCharMov)
	{
		if (UTCharMov->bIsSettingUpFirstReplayMove)
		{
			UTCharMov->CurrentMultiJumpCount = SavedMultiJumpCount;
			UTCharMov->CurrentWallDodgeCount = SavedWallDodgeCount;
			UTCharMov->SprintStartTime = SavedSprintStartTime;
			UTCharMov->DodgeResetTime = SavedDodgeResetTime;
			UTCharMov->FloorSlideEndTime = SavedFloorSlideEndTime;
			UTCharMov->bJumpAssisted = bSavedJumpAssisted;
			UTCharMov->bIsDodging = bSavedIsDodging;
			//UE_LOG(UT, Warning, TEXT("First move %f bIsDodging %d"), TimeStamp, UTCharMov->bIsDodging);
		}
		else
		{
			/*
			// warn if any of these changed (can be legit)
			if (SavedMultiJumpCount != UTCharMov->CurrentMultiJumpCount)
			{
			UE_LOG(UTNet, Warning, TEXT("prep move %f SavedMultiJumpCount from %d to %d"), TimeStamp, SavedMultiJumpCount, UTCharMov->CurrentMultiJumpCount);
			}
			if (SavedWallDodgeCount != UTCharMov->CurrentWallDodgeCount)
			{
			UE_LOG(UTNet, Warning, TEXT("prep move %f SavedWallDodgeCount from %d to %d"), TimeStamp, SavedWallDodgeCount, UTCharMov->CurrentWallDodgeCount);
			}
			if (SavedSprintStartTime != UTCharMov->SprintStartTime)
			{
			UE_LOG(UTNet, Warning, TEXT("prep move %f SavedSprintStartTime from %f to %f"), TimeStamp, SavedSprintStartTime, UTCharMov->SprintStartTime);
			}
			if (SavedDodgeResetTime != UTCharMov->DodgeResetTime)
			{
			UE_LOG(UTNet, Warning, TEXT("prep move %f SavedDodgeResetTime from %f to %f"), TimeStamp, SavedDodgeResetTime, UTCharMov->DodgeResetTime);
			}
			if (SavedFloorSlideEndTime != UTCharMov->FloorSlideEndTime)
			{
			UE_LOG(UTNet, Warning, TEXT("prep move %f SavedDodgeResetTime from %f to %f"), TimeStamp, SavedFloorSlideEndTime, UTCharMov->FloorSlideEndTime);
			}
			if (SavedWallDodgeCount != UTCharMov->CurrentWallDodgeCount)
			{
			UE_LOG(UTNet, Warning, TEXT("prep move %f SavedWallDodgeCount from %d to %d"), TimeStamp, SavedWallDodgeCount, UTCharMov->CurrentWallDodgeCount);
			}
			if (bSavedJumpAssisted != UTCharMov->bJumpAssisted)
			{
			UE_LOG(UTNet, Warning, TEXT("prep move %f bSavedJumpAssisted from %d to %d"), TimeStamp, bSavedJumpAssisted, UTCharMov->bJumpAssisted);
			}
			if (bSavedIsDodging != UTCharMov->bIsDodging)
			{
			UE_LOG(UTNet, Warning, TEXT("prep move %f bSavedIsDodging from %d to %d"), TimeStamp, bSavedIsDodging, UTCharMov->bIsDodging);
			}
			*/
			// these may have changed in the course of replaying saved moves.  Save new value, since we reset to this before the first one is played
			// @TODO FIXMESTEVE should I update all properties (non input)?
			SavedMultiJumpCount = UTCharMov->CurrentMultiJumpCount;
			SavedWallDodgeCount = UTCharMov->CurrentWallDodgeCount;
			SavedSprintStartTime = UTCharMov->SprintStartTime;
			SavedDodgeResetTime = UTCharMov->DodgeResetTime;
			SavedFloorSlideEndTime = UTCharMov->FloorSlideEndTime;
			bSavedJumpAssisted = UTCharMov->bJumpAssisted;
			bSavedIsDodging = UTCharMov->bIsDodging;
		}
	}
}

void FSavedMove_UTCharacter::PostUpdate(ACharacter* Character, FSavedMove_Character::EPostUpdateMode PostUpdateMode)
{
	// set flag if weapon is shooting on client this frame not from new fire press/release (to keep client and server synchronized)
	UUTCharacterMovement* UTCharMovement = Character ? Cast<UUTCharacterMovement>(Character->GetCharacterMovement()) : NULL;
	bShotSpawned = UTCharMovement->bShotSpawned;
	MovementMode = UTCharMovement->PackNetworkMovementMode(); // @TODO FIXMESTEVE SHOULD BE IN Engine version as well
	Super::PostUpdate(Character, PostUpdateMode);
}

FNetworkPredictionData_Client* UUTCharacterMovement::GetPredictionData_Client() const
{
	// Should only be called on client in network games
	check(PawnOwner != NULL);
	check(PawnOwner->Role < ROLE_Authority);

	// once the NM_Client bug is fixed during map transition, should re-enable this
	//check(GetNetMode() == NM_Client);

	if (!ClientPredictionData)
	{
		UUTCharacterMovement* MutableThis = const_cast<UUTCharacterMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_UTChar(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f; // 2X character capsule radius
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}

	return ClientPredictionData;
}
