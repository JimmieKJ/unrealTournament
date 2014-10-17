// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCharacterMovement.h"
#include "UTBot.h"
#include "UTAIAction.h"
#include "UTAIAction_WaitForMove.h"
#include "UTAIAction_WaitForLanding.h"
#include "UTDroppedPickup.h"

AUTBot::AUTBot(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bWantsPlayerState = true;
	RotationRate = FRotator(300.0f, 300.0f, 0.0f);
	SightRadius = 20000.0f;
	PeripheralVision = 0.7f;
	TrackingReactionTime = 0.25f;

	WaitForMoveAction = PCIP.CreateDefaultSubobject<UUTAIAction_WaitForMove>(this, FName(TEXT("WaitForMove")));
	WaitForLandingAction = PCIP.CreateDefaultSubobject<UUTAIAction_WaitForLanding>(this, FName(TEXT("WaitForLanding")));
}

float FBestInventoryEval::Eval(APawn* Asker, const FNavAgentProperties& AgentProps, const UUTPathNode* Node, const FVector& EntryLoc, int32 TotalDistance)
{
	float BestNodeWeight = 0.0f;
	for (TWeakObjectPtr<AActor> TestActor : Node->POIs)
	{
		if (TestActor.IsValid())
		{
			AUTPickup* TestPickup = Cast<AUTPickup>(TestActor.Get());
			if (TestPickup != NULL)
			{
				if (TestPickup->State.bActive) // TODO: flag for pickup timing
				{
					float NewWeight = TestPickup->BotDesireability(Asker, TotalDistance) / FMath::Max<float>(1.0f, float(TotalDistance) + (TestPickup->GetActorLocation() - EntryLoc).Size());
					if (NewWeight > BestNodeWeight)
					{
						BestNodeWeight = NewWeight;
						if (NewWeight > BestWeight)
						{
							BestPickup = TestPickup;
							BestWeight = NewWeight;
						}
					}
				}
			}
			else
			{
				AUTDroppedPickup* TestDrop = Cast<AUTDroppedPickup>(TestActor.Get());
				if (TestDrop != NULL)
				{
					float NewWeight = TestDrop->BotDesireability(Asker, TotalDistance) / FMath::Max<float>(1.0f, float(TotalDistance) + (TestDrop->GetActorLocation() - EntryLoc).Size());
					if (NewWeight > BestNodeWeight)
					{
						BestNodeWeight = NewWeight;
						if (NewWeight > BestWeight)
						{
							BestPickup = TestDrop;
							BestWeight = NewWeight;
						}
					}
				}
			}
		}
	}
	return BestNodeWeight;
}
bool FBestInventoryEval::GetRouteGoal(AActor*& OutGoal, FVector& OutGoalLoc) const
{
	if (BestPickup != NULL)
	{
		OutGoal = BestPickup;
		OutGoalLoc = OutGoal->GetActorLocation();
		return true;
	}
	else
	{
		return false;
	}
}

void AUTBot::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	UTChar = Cast<AUTCharacter>(GetPawn());
}

void AUTBot::Possess(APawn* InPawn)
{
	Super::Possess(InPawn);

	ClearMoveTarget();
	
	// set weapon timer, if not already
	GetWorldTimerManager().SetTimer(this, &AUTBot::CheckWeaponFiringTimed, 1.2f - 0.09f * FMath::Min<float>(10.0f, Skill + Personality.ReactionTime), true);
}

void AUTBot::Destroyed()
{
	if (UTChar != NULL)
	{
		UTChar->PlayerSuicide();
	}
	// TODO: vehicles
	else
	{
		UnPossess();
	}
	
	Super::Destroyed();
}

uint8 AUTBot::GetTeamNum() const
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	return (PS != NULL && PS->Team != NULL) ? PS->Team->TeamIndex : 255;
}

void AUTBot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (NavData == NULL)
	{
		NavData = GetUTNavData(GetWorld());
	}
	APawn* MyPawn = GetPawn();
	if (MyPawn == NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		if (PS == NULL || PS->RespawnTime <= 0.0f)
		{
			GetWorld()->GetAuthGameMode()->RestartPlayer(this);
		}
		MyPawn = GetPawn();
	}
	if (MyPawn != NULL && NavData != NULL)
	{
		if (MoveTarget.IsValid())
		{
			if (NavData->HasReachedTarget(MyPawn, *MyPawn->GetNavAgentProperties(), MoveTarget, CurrentPath))
			{
				// reached
				ClearMoveTarget();
			}
			else
			{
				MoveTimer -= DeltaTime;
				if (MoveTimer < 0.0f && (GetCharacter() == NULL || GetCharacter()->CharacterMovement == NULL || GetCharacter()->CharacterMovement->MovementMode != MOVE_Falling))
				{
					// timed out
					ClearMoveTarget();
				}
				else
				{
					// TODO: need some sort of failure checks in here

					// clear points that we've reached or passed
					const FVector MyLoc = MyPawn->GetActorLocation();
					float DistFromTarget = (MoveTarget.GetLocation(MyPawn) - MyLoc).Size();
					const FVector Extent = MyPawn->GetSimpleCollisionCylinderExtent();
					FBox MyBox(0);
					MyBox += MyLoc + Extent;
					MyBox += MyLoc - Extent;
					if (bAdjusting && MyBox.IsInside(AdjustLoc))
					{
						bAdjusting = false;
					}
					if (!bAdjusting)
					{
						for (int32 i = MoveTargetPoints.Num() - 2; i >= 0; i--)
						{
							if (MyBox.IsInside(MoveTargetPoints[i].Get()))
							{
								LastReachedMovePoint = MoveTargetPoints[i].Get();
								MoveTargetPoints.RemoveAt(0, i + 1);
								break;
							}
							// if path requires complex movement (jumps, etc) then require touching second to last point
							// since the final part of the path may require more precision
							if (i < MoveTargetPoints.Num() - 2 || CurrentPath.ReachFlags == 0)
							{
								FVector HitLoc;
								if (DistFromTarget < (MoveTarget.GetLocation(MyPawn) - MoveTargetPoints[i].Get()).Size() && !NavData->RaycastWithZCheck(GetNavAgentLocation(), MoveTargetPoints[i].Get(), Extent.Z * 1.5f, HitLoc))
								{
									LastReachedMovePoint = MoveTargetPoints[i].Get();
									MoveTargetPoints.RemoveAt(0, i + 1);
									break;
								}
							}
						}
					}

					// failure checks
					FVector MovePoint = GetMovePoint();
					if ((MovePoint - MyLoc).Size2D() < MyPawn->GetSimpleCollisionRadius())
					{
						static FName NAME_AIZCheck(TEXT("AIZCheck"));

						float ZDiff = MyLoc.Z - MovePoint.Z;
						bool bZFail = false;
						if (!(CurrentPath.ReachFlags & R_JUMP))
						{
							bZFail = FMath::Abs<float>(ZDiff) < MyPawn->GetSimpleCollisionHalfHeight();
						}
						else if (ZDiff < -MyPawn->GetSimpleCollisionHalfHeight())
						{
							bZFail = true;
						}
						else
						{
							// for jump/fall path make sure we don't just need to get closer to the edge
							FVector TargetPoint = MovePoint;
							bZFail = GetWorld()->LineTraceTest(FVector(TargetPoint.X, TargetPoint.Y, MyLoc.Z), TargetPoint, ECC_Pawn, FCollisionQueryParams(NAME_AIZCheck, false, MyPawn));
						}
						if (bZFail)
						{
							if (GetCharacter() != NULL)
							{
								if (GetCharacter()->CharacterMovement->MovementMode == MOVE_Walking)
								{
									// failed - directly above or below target
									ClearMoveTarget();
								}
							}
							else if (GetWorld()->SweepTest(MyLoc, MovePoint, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(MyPawn->GetSimpleCollisionCylinderExtent() * FVector(0.9f, 0.9f, 0.1f)), FCollisionQueryParams(NAME_AIZCheck, false, MyPawn)))
							{
								// failed - directly above or below target
								ClearMoveTarget();
							}
						}
					}
				}
			}
		}

		// check for enemy visibility
		SightCounter -= DeltaTime;
		if (SightCounter < 0.0f)
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
			{
				if (It->IsValid())
				{
					AController* C = It->Get();
					if (C->GetPawn() != NULL && (bSeeFriendly || GS == NULL || !GS->OnSameTeam(C, this)) && CanSee(C->GetPawn(), true) && (!AreAIIgnoringPlayers() || !C->IsA(APlayerController::StaticClass())))
					{
						SeePawn(C->GetPawn());
					}
				}
			}
			SightCounter += 0.15f + 0.1f * FMath::SRand();
		}

		// process current action
		if (CurrentAction == NULL)
		{
			WhatToDoNext();
		}
		else if (CurrentAction->Update(DeltaTime))
		{
			if (CurrentAction != NULL) // could have ended itself...
			{
				CurrentAction->Ended(false);
				CurrentAction = NULL;
			}
			WhatToDoNext();
		}
		// start new action, if requested
		if (bPendingWhatToDoNext)
		{
			bExecutingWhatToDoNext = true;
			ExecuteWhatToDoNext();
			bExecutingWhatToDoNext = false;
			bPendingWhatToDoNext = false;
			if (CurrentAction == NULL)
			{
				UE_LOG(UT, Warning, TEXT("%s (%s) failed to get an action from ExecuteWhatToDoNext()"), *GetName(), *PlayerState->PlayerName);
			}
		}

		if (MoveTarget.IsValid())
		{
			if (MoveTargetPoints.Num() == 0)
			{
				// TODO: raycast for direct reachability?
				float TotalDistance = 0.0f;
				if (NavData->GetMovePoints(MyPawn->GetNavAgentLocation(), MyPawn, *MyPawn->GetNavAgentProperties(), MoveTarget, RouteCache, MoveTargetPoints, CurrentPath, &TotalDistance))
				{
					MoveTimer = TotalDistance / MyPawn->GetMovementComponent()->GetMaxSpeed() + 1.0f;
					if (Cast<APawn>(MoveTarget.Actor.Get()) != NULL)
					{
						MoveTimer += 2.0f; // TODO: maybe do this for any moving target?
					}
				}
				else
				{
					ClearMoveTarget();
				}
			}
			if (MoveTarget.IsValid())
			{
				FVector TargetLoc = GetMovePoint();
				if (Target != NULL && Target != Enemy && LineOfSightTo(Target))
				{
					SetFocus(Target);
				}
				else  if (Enemy != NULL && LineOfSightTo(Enemy)) // TODO: cache this
				{
					SetFocus(Enemy);
				}
				else
				{
					if (MoveTarget.Actor.IsValid())
					{
						SetFocus(MoveTarget.Actor.Get());
					}
					else
					{
						SetFocalPoint(MoveTarget.GetLocation(MyPawn)); // hmm, better in some situations, worse in others. Maybe periodically trace? Or maybe it doesn't matter because we'll have an enemy most of the time
					}
				}
				if (GetCharacter() != NULL && GetCharacter()->CharacterMovement != NULL)
				{
					GetCharacter()->CharacterMovement->bCanWalkOffLedges = !CurrentPath.IsSet() || (CurrentPath.ReachFlags & R_JUMP);
				}
				if (GetCharacter()->CharacterMovement->MovementMode == MOVE_Falling && GetCharacter()->CharacterMovement->AirControl > 0.0f)
				{
					// figure out desired 2D velocity and set air control to achieve that
					float GravityZ = GetCharacter()->CharacterMovement->GetGravityZ();
					float Determinant = FMath::Square(GetCharacter()->CharacterMovement->Velocity.Z) - 2.0 * GravityZ * (MyPawn->GetActorLocation().Z - TargetLoc.Z);
					if (Determinant >= 0.0f)
					{
						Determinant = FMath::Sqrt(Determinant);
						float Time = (-GetCharacter()->CharacterMovement->Velocity.Z + Determinant) / GravityZ;
						if (Time <= 0.0f)
						{
							Time = (-GetCharacter()->CharacterMovement->Velocity.Z - Determinant) / GravityZ;
						}
						if (Time > 0.0f)
						{
							FVector DesiredVel2D = (TargetLoc - MyPawn->GetActorLocation()) / Time;
							FVector NewAccel = (DesiredVel2D - GetCharacter()->CharacterMovement->Velocity) / DeltaTime / GetCharacter()->CharacterMovement->AirControl;
							NewAccel.Z = 0.0f;
							MyPawn->GetMovementComponent()->AddInputVector(NewAccel.SafeNormal() * (NewAccel.Size() / GetCharacter()->CharacterMovement->MaxWalkSpeed));
						}
					}
				}
				// do nothing if path says we need to wait
				else if (CurrentPath.Spec == NULL || !CurrentPath.Spec->WaitForMove(GetPawn()))
				{
					MyPawn->GetMovementComponent()->AddInputVector((TargetLoc - MyPawn->GetActorLocation()).SafeNormal2D());
				}
			}
		}
	}
}

void AUTBot::SetMoveTarget(const FRouteCacheItem& NewMoveTarget, const TArray<FComponentBasedPosition>& NewMovePoints)
{
	MoveTarget = NewMoveTarget;
	MoveTargetPoints = NewMovePoints;
	bAdjusting = false;
	CurrentPath = FUTPathLink();
	if (NavData == NULL || !NavData->GetPolyCenter(NavData->FindNearestPoly(GetPawn()->GetNavAgentLocation(), GetPawn()->GetSimpleCollisionCylinderExtent()), LastReachedMovePoint))
	{
		LastReachedMovePoint = GetPawn()->GetActorLocation();
	}
	// default movement code will generate points and set MoveTimer, this just makes sure we don't abort before even getting there
	MoveTimer = FMath::Max<float>(MoveTimer, 1.0f);
}

void AUTBot::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	if (GetPawn() != NULL)
	{
		if (MoveTarget.IsValid())
		{
			for (int32 i = 0; i < MoveTargetPoints.Num() - 1; i++)
			{
				DrawDebugLine(GetWorld(), (i == 0) ? GetPawn()->GetActorLocation() : MoveTargetPoints[i - 1].Get(), MoveTargetPoints[i].Get(), FColor(0, 255, 0));
			}
			DrawDebugLine(GetWorld(), (MoveTargetPoints.Num() > 1) ? MoveTargetPoints[MoveTargetPoints.Num() - 2].Get() : GetPawn()->GetActorLocation(), MoveTarget.GetLocation(GetPawn()), FColor(0, 255, 0));
			if (bAdjusting)
			{
				DrawDebugLine(GetWorld(), GetPawn()->GetActorLocation(), AdjustLoc, FColor(255, 0, 0));
			}
		}
		for (const FRouteCacheItem& RoutePoint : RouteCache)
		{
			DrawDebugSphere(GetWorld(), RoutePoint.GetLocation(GetPawn()), 16.0f, 8, FColor(0, 255, 0));
		}
	}
}

void AUTBot::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	// Look toward focus
	FVector FocalPoint = GetFocalPoint();
	if (!FocalPoint.IsZero())
	{
		APawn* P = GetPawn();
		if (P != NULL)
		{
			const float WorldTime = GetWorld()->TimeSeconds;

			const TArray<FSavedPosition>* SavedPosPtr = NULL;
			AUTCharacter* TargetP = Cast<AUTCharacter>(GetFocusActor());
			AUTWeapon* MyWeap = (UTChar != NULL) ? UTChar->GetWeapon() : NULL;
			if (TargetP != NULL && TargetP->SavedPositions.Num() > 0 && TargetP->SavedPositions[0].Time <= WorldTime - TrackingReactionTime)
			{
				SavedPosPtr = &TargetP->SavedPositions;
			}
			if (SavedPosPtr != NULL)
			{
				const TArray<FSavedPosition>& SavedPositions = *SavedPosPtr;
				// determine his position and velocity at the appropriate point in the past
				for (int32 i = 1; i < SavedPositions.Num(); i++)
				{
					if (SavedPositions[i].Time > WorldTime - TrackingReactionTime)
					{
						FVector TargetLoc = SavedPositions[i - 1].Position + (SavedPositions[i].Position - SavedPositions[i - 1].Position) * (WorldTime - TrackingReactionTime - SavedPositions[i - 1].Time) / (SavedPositions[i].Time - SavedPositions[i - 1].Time);
						const FVector TrackedVelocity = SavedPositions[i - 1].Velocity + (SavedPositions[i].Velocity - SavedPositions[i - 1].Velocity) * (WorldTime - TrackingReactionTime - SavedPositions[i - 1].Time) / (SavedPositions[i].Time - SavedPositions[i - 1].Time);
						
						TargetLoc = TargetLoc + TrackedVelocity * TrackingReactionTime;
						if (MyWeap != NULL)
						{
							if (MyWeap->CanAttack(TargetP, TargetLoc, false, !bPickNewFireMode, NextFireMode, FocalPoint))
							{
								bPickNewFireMode = false;
							}
							else
							{
								FocalPoint = TargetLoc; // LastSeenLoc ???
							}
						}
						else
						{
							FocalPoint = TargetLoc;
						}

						// TODO: leading for projectiles, etc
						break;
					}
				}
			}
			else if (MyWeap != NULL && Target != NULL && GetFocusActor() == Target)
			{
				FVector TargetLoc = GetFocusActor()->GetTargetLocation();
				if (MyWeap->CanAttack(GetFocusActor(), TargetLoc, false, !bPickNewFireMode, NextFireMode, FocalPoint))
				{
					bPickNewFireMode = false;
				}
			}

			FVector Direction = FocalPoint - P->GetActorLocation();
			FRotator DesiredRotation = Direction.Rotation();

			// Don't pitch view of walking pawns when simply traversing path and not looking at a target
			if (GetPawn()->GetMovementComponent() && GetPawn()->GetMovementComponent()->IsMovingOnGround() && GetFocusActor() == NULL)
			{
				DesiredRotation.Pitch = 0.f;
			}
			DesiredRotation.Yaw = FRotator::ClampAxis(DesiredRotation.Yaw);

			FRotator NewControlRotation(FMath::FixedTurn(ControlRotation.Pitch, DesiredRotation.Pitch, RotationRate.Pitch * DeltaTime), FMath::FixedTurn(ControlRotation.Yaw, DesiredRotation.Yaw, RotationRate.Yaw * DeltaTime), 0);

			SetControlRotation(NewControlRotation);
			if (bUpdatePawn)
			{
				P->FaceRotation(NewControlRotation, DeltaTime);
			}
		}
	}
}

void AUTBot::NotifyWalkingOffLedge()
{
	// jump if needed by path
	if (GetCharacter() != NULL && MoveTarget.IsValid() && (CurrentPath.ReachFlags & R_JUMP)) // TODO: maybe also if chasing enemy?
	{
		FVector Diff = GetMovePoint() - GetCharacter()->GetActorLocation();
		float XYTime = Diff.Size2D() / GetCharacter()->CharacterMovement->MaxWalkSpeed;
		float DesiredJumpZ = Diff.Z / XYTime - 0.5f * GetCharacter()->CharacterMovement->GetGravityZ() * XYTime;
		// TODO: if high skill also check if path is walkable from simple fall location to dest via navmesh raytrace to minimize in air time
		if (DesiredJumpZ > 0.0f)
		{
			// try forward dodge instead if target is a little too far but is below and path is clear
			bool bDodged = false;
			if (DesiredJumpZ > GetCharacter()->CharacterMovement->JumpZVelocity && GetUTChar() != NULL && GetUTChar()->UTCharacterMovement->CanDodge())
			{
				float DodgeXYTime = Diff.Size2D() / GetUTChar()->UTCharacterMovement->DodgeImpulseHorizontal;
				float DodgeDesiredJumpZ = Diff.Z / DodgeXYTime - 0.5f * GetCharacter()->CharacterMovement->GetGravityZ() * DodgeXYTime;
				// TODO: need FRouteCacheItem function that conditionally Z adjusts
				FCollisionQueryParams TraceParams(FName(TEXT("Dodge")), false, GetPawn());
				if (DodgeDesiredJumpZ <= GetUTChar()->UTCharacterMovement->DodgeImpulseVertical && !GetWorld()->LineTraceTest(GetCharacter()->GetActorLocation(), GetMovePoint() + FVector(0.0f, 0.0f, 60.0f), ECC_Pawn, TraceParams))
				{
					// TODO: very minor cheat here - non-cardinal dodge
					//		to avoid would need to add the ability for the AI to reject the fall in the first place and delay until it rotates to correct rotation
					//FRotationMatrix YawMat(FRotator(0.f, GetUTChar()->GetActorRotation().Yaw, 0.f));
					FRotationMatrix YawMat(FRotator(0.f, Diff.Rotation().Yaw, 0.f));
					// forward dodge
					FVector X = YawMat.GetScaledAxis(EAxis::X).SafeNormal();
					FVector Y = YawMat.GetScaledAxis(EAxis::Y).SafeNormal();
					GetUTChar()->Dodge(X, Y);
					bDodged = true;
				}
			}
			if (!bDodged)
			{
				GetCharacter()->CharacterMovement->Velocity = Diff.SafeNormal2D() * (GetCharacter()->CharacterMovement->MaxWalkSpeed * FMath::Min<float>(1.0f, DesiredJumpZ / FMath::Max<float>(GetCharacter()->CharacterMovement->JumpZVelocity, 1.0f)));
				GetCharacter()->CharacterMovement->DoJump(false);
			}
		}
		else
		{
			// clamp initial XY speed if target is directly below
			float ZTime = FMath::Sqrt(Diff.Z / (0.5f * GetCharacter()->CharacterMovement->GetGravityZ()));
			GetCharacter()->CharacterMovement->Velocity = Diff.SafeNormal2D() * FMath::Min<float>(GetCharacter()->CharacterMovement->MaxWalkSpeed, Diff.Size2D() / ZTime);
		}
	}
}

void AUTBot::NotifyMoveBlocked(const FHitResult& Impact)
{
	if (GetCharacter() != NULL)
	{
		if (GetCharacter()->CharacterMovement->MovementMode == MOVE_Walking)
		{
			const FVector MovePoint = GetMovePoint();
			const FVector MyLoc = GetCharacter()->GetActorLocation();
			// get as close as possible to path
			bool bGotAdjustLoc = false;
			if (!bAdjusting && CurrentPath.EndPoly != INVALID_NAVNODEREF)
			{
				FVector ClosestPoint = FMath::ClosestPointOnSegment(MyLoc, LastReachedMovePoint, MovePoint);
				if ((ClosestPoint - MyLoc).SizeSquared() > FMath::Square(GetCharacter()->CapsuleComponent->GetUnscaledCapsuleRadius()))
				{
					FCollisionQueryParams Params(FName(TEXT("MoveBlocked")), false, GetPawn());
					// try directly back to path center, then try backing up a bit towards path start
					FVector Side = (MovePoint - MyLoc).SafeNormal() ^ FVector(0.0f, 0.0f, 1.0f);
					if ((Side | (ClosestPoint - MyLoc).SafeNormal()) < 0.0f)
					{
						Side *= -1.0f;
					}
					FVector TestLocs[] = { ClosestPoint, ClosestPoint + (LastReachedMovePoint - ClosestPoint).SafeNormal() * GetCharacter()->CapsuleComponent->GetUnscaledCapsuleRadius() * 2.0f,
											MyLoc + Side * (ClosestPoint - MyLoc).Size() };
					for (int32 i = 0; i < ARRAY_COUNT(TestLocs); i++)
					{
						if (!GetWorld()->LineTraceTest(MyLoc, TestLocs[i], ECC_Pawn, Params))
						{
							AdjustLoc = TestLocs[i];
							bAdjusting = true;
							bGotAdjustLoc = true;
							break;
						}
					}
				}
			}
			if (!bGotAdjustLoc)
			{
				if (MoveTarget.IsValid() && (CurrentPath.ReachFlags & R_JUMP)) // TODO: maybe also if chasing enemy?)
				{
					FVector Diff = MovePoint - MyLoc;
					// make sure hit wall in actual direction we should be going (commonly this check fails when multiple jumps are required and AI hasn't adjusted velocity to new direction yet)
					if (((Impact.Normal * -1.0f) | Diff.SafeNormal()) > 0.0f)
					{
						float XYTime = Diff.Size2D() / GetCharacter()->CharacterMovement->MaxWalkSpeed;
						float DesiredJumpZ = Diff.Z / XYTime - 0.5 * GetCharacter()->CharacterMovement->GetGravityZ() * XYTime;
						if (DesiredJumpZ > 0.0f)
						{
							GetCharacter()->CharacterMovement->Velocity = Diff.SafeNormal2D() * (GetCharacter()->CharacterMovement->MaxWalkSpeed * FMath::Min<float>(1.0f, DesiredJumpZ / FMath::Max<float>(GetCharacter()->CharacterMovement->JumpZVelocity, 1.0f)));
						}
						GetCharacter()->CharacterMovement->DoJump(false);
					}
				}
				else if (Impact.Time <= 0.0f)
				{
					ClearMoveTarget();
				}
			}
		}
		else if (GetCharacter()->CharacterMovement->MovementMode == MOVE_Falling)
		{
			if (UTChar != NULL && UTChar->CanDodge())
			{
				if (bPlannedWallDodge)
				{
					// dodge already checked, just do it
					UTChar->Dodge(Impact.Normal, (Impact.Normal ^ FVector(0.0f, 0.0f, 1.0f)).SafeNormal());
					// possibly maintain readiness for a second wall dodge if we hit another wall
					bPlannedWallDodge = FMath::FRand() < Personality.Jumpiness || (Personality.Jumpiness >= 0.0 && FMath::FRand() < (Skill + Personality.Tactics) * 0.1f);
				}
				else
				{
					// TODO: maybe wall dodge
				}
			}
		}
	}
}

void AUTBot::NotifyLanded(const FHitResult& Hit)
{
	bPlannedWallDodge = false;
}

float AUTBot::RateWeapon(AUTWeapon* W)
{
	if (W != NULL && W->GetUTOwner() == GetUTChar() && GetUTChar() != NULL)
	{
		float Rating = W->GetAISelectRating();
		// prefer favorite weapon
		if (IsFavoriteWeapon(W->GetClass()))
		{
			Rating += 0.1f;
		}
		// slightly prefer current weapon (account for weapon switch cost, avoid oscillation, etc)
		if (W == GetUTChar()->GetWeapon())
		{
			Rating += 0.05f;
		}
		return Rating;
	}
	else
	{
		return -1000.0f;
	}
}

void AUTBot::SwitchToBestWeapon()
{
	if (GetUTChar() != NULL)
	{
		float BestRating = -100.0f;
		AUTWeapon* Best = NULL;

		for (TInventoryIterator<AUTWeapon> It(GetUTChar()); It; ++It)
		{
			float NewRating = RateWeapon(*It);
			if (NewRating > BestRating)
			{
				Best = *It;
				BestRating = NewRating;
			}
		}

		GetUTChar()->SwitchWeapon(Best);
	}
}

bool AUTBot::IsFavoriteWeapon(TSubclassOf<AUTWeapon> TestClass)
{
	return (TestClass != NULL && TestClass->IsChildOf(Personality.FavoriteWeapon));
}

bool AUTBot::NeedsWeapon()
{
	return (GetUTChar() != NULL && (GetUTChar()->GetWeapon() == NULL || GetUTChar()->GetWeapon()->BaseAISelectRating < 0.5f));
}

void AUTBot::CheckWeaponFiring(bool bFromWeapon)
{
	if (UTChar == NULL)
	{
		GetWorldTimerManager().ClearTimer(this, &AUTBot::CheckWeaponFiringTimed); // timer will get restarted in Possess() if we get a new Pawn
	}
	else if (UTChar->GetWeapon() != NULL && (bFromWeapon || !UTChar->GetWeapon()->IsFiring())) // if weapon is firing, it should query bot when it's done for better responsiveness than a timer
	{
		AActor* TestTarget = Target;
		if (TestTarget == NULL)
		{
			// TODO: check time since last enemy loc update versus reaction time
			TestTarget = Enemy;
		}
		// TODO: if no target, ask weapon if it should fire anyway (mine layers, traps, fortifications, etc)
		FVector OptimalLoc;
		// TODO: think about how to prevent Focus/Target/Enemy mismatches
		if (TestTarget != NULL && GetFocusActor() == TestTarget && UTChar->GetWeapon()->CanAttack(TestTarget, TestTarget->GetTargetLocation(), false, true, NextFireMode, OptimalLoc) && (!NeedToTurn(OptimalLoc) || UTChar->GetWeapon()->IsChargedFireMode(NextFireMode)))
		{
			for (uint8 i = 0; i < UTChar->GetWeapon()->GetNumFireModes(); i++)
			{
				if (i == NextFireMode)
				{
					if (!UTChar->IsPendingFire(i))
					{
						UTChar->StartFire(i);
					}
				}
				else if (UTChar->IsPendingFire(i))
				{
					UTChar->StopFire(i);
				}

				// Start fire can cause us to lose our character
				if (UTChar == nullptr)
				{
					break;
				}
			}
		}
		else
		{
			UTChar->StopFiring();
		}
		bPickNewFireMode = true;
	}
}
void AUTBot::CheckWeaponFiringTimed()
{
	CheckWeaponFiring(false);
}

bool AUTBot::NeedToTurn(const FVector& TargetLoc)
{
	if (GetPawn() == NULL)
	{
		return false;
	}
	else
	{
		// we're intentionally disregarding the weapon's start position here, since it may change based on its firing offset, nearby geometry if that offset is outside the cylinder, etc
		// we'll correct for the discrepancy in GetAdjustedAim() while firing
		const FVector StartLoc = GetPawn()->GetActorLocation();
		return ((TargetLoc - StartLoc).SafeNormal() | GetControlRotation().Vector()) < 0.93f + 0.0085 * FMath::Clamp<float>(Skill, 0.0, 7.0);
	}
}

void AUTBot::StartNewAction(UUTAIAction* NewAction)
{
	if (CurrentAction != NULL)
	{
		CurrentAction->Ended(true);
	}
	CurrentAction = NewAction;
	if (CurrentAction != NULL)
	{
		CurrentAction->Started();
	}
}

bool AUTBot::FindInventoryGoal(float MinWeight)
{
	if (LastFindInventoryTime == GetWorld()->TimeSeconds && LastFindInventoryWeight >= MinWeight)
	{
		return false;
	}
	/*else if (GameHasNoInventory() || PawnCantPickUpInventory())
	{
		return false;
	}
	*/
	else
	{
		LastFindInventoryTime = GetWorld()->TimeSeconds;
		LastFindInventoryWeight = MinWeight;

		FBestInventoryEval NodeEval;
		return NavData->FindBestPath(GetPawn(), *GetPawn()->GetNavAgentProperties(), NodeEval, GetPawn()->GetNavAgentLocation(), MinWeight, false, RouteCache);
	}
}

void AUTBot::WhatToDoNext()
{
	ensure(!bExecutingWhatToDoNext);

	bPendingWhatToDoNext = true;
}

void AUTBot::ExecuteWhatToDoNext()
{
	SwitchToBestWeapon();

	if (GetCharacter() != NULL && GetCharacter()->CharacterMovement->MovementMode == MOVE_Falling)
	{
		StartNewAction(WaitForLandingAction);
	}
	else
	{
		if (Enemy != NULL && GetUTChar() != NULL && GetUTChar()->GetWeapon() != NULL && GetUTChar()->GetWeapon()->BaseAISelectRating >= 0.5f && LineOfSightTo(Enemy))
		{
			float Weight = 0.0f;
			FSingleEndpointEval NodeEval(Enemy);
			NavData->FindBestPath(GetPawn(), *GetPawn()->GetNavAgentProperties(), NodeEval, GetPawn()->GetNavAgentLocation(), Weight, true, RouteCache);
			if (RouteCache.Num() > 0)
			{
				SetMoveTarget(RouteCache[0]);
				StartNewAction(WaitForMoveAction);
			}
		}
		if (CurrentAction == NULL)
		{
			if (FindInventoryGoal(0.0f))
			{
				SetMoveTarget(RouteCache[0]);
				StartNewAction(WaitForMoveAction);
			}
		}
		
		// FALLBACK: just wander randomly
		if (CurrentAction == NULL)
		{
			FRandomDestEval NodeEval;
			float Weight = 0.0f;
			if (NavData->FindBestPath(GetPawn(), *GetPawn()->GetNavAgentProperties(), NodeEval, GetPawn()->GetNavAgentLocation(), Weight, true, RouteCache))
			{
				SetMoveTarget(RouteCache[0]);
				StartNewAction(WaitForMoveAction);
			}
			else
			{
				SetMoveTargetDirect(FRouteCacheItem(GetPawn()->GetActorLocation() + FMath::VRand() * FVector(500.0f, 500.0f, 0.0f), INVALID_NAVNODEREF));
				StartNewAction(WaitForMoveAction);
			}
		}
	}
}

void AUTBot::UTNotifyKilled(AController* Killer, AController* KilledPlayer, APawn* KilledPawn, const UDamageType* DamageType)
{
	if (KilledPawn == Enemy)
	{
		SetEnemy(NULL);
		// TODO: maybe taunt
	}
}

void AUTBot::NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, const FDamageEvent& DamageEvent)
{
	if (InstigatedBy != NULL && InstigatedBy != this && InstigatedBy->GetPawn() != NULL && (!AreAIIgnoringPlayers() || Cast<APlayerController>(InstigatedBy) == NULL) && (Enemy == NULL || !LineOfSightTo(Enemy)))
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(InstigatedBy, this))
		{
			SetEnemy(InstigatedBy->GetPawn());
			WhatToDoNext();
		}
	}
}

void AUTBot::ReceiveProjWarning(AUTProjectile* Incoming)
{
	if (Incoming != NULL && !Incoming->GetVelocity().IsZero() && GetPawn() != NULL)
	{
		// bots may duck if not falling or swimming
		if (Skill >= 2.0f && Enemy != NULL) // TODO: if 1 on 1 (T)DM be more alert? maybe enemy will usually be set so doesn't matter
		{
			//LastUnderFire = WorldInfo.TimeSeconds;
			//if (WorldInfo.TimeSeconds - LastWarningTime < 0.5)
			//	return;
			//LastWarningTime = WorldInfo.TimeSeconds;

			// TODO: should adjust target location if projectile can explode in air like shock combo (i.e. account for damage radius and dodge earlier)
			float ProjTime = Incoming->GetTimeToLocation(GetPawn()->GetActorLocation());
			// check if tight FOV
			bool bShouldDodge = true;
			if (ProjTime < 1.2 || WarningProj != NULL)
			{
				FVector EnemyDir = Incoming->GetActorLocation() - GetPawn()->GetActorLocation();
				EnemyDir.Z = 0;
				FRotationMatrix R(GetControlRotation());
				FVector X = R.GetScaledAxis(EAxis::X);
				X.Z = 0;
				if ((EnemyDir.SafeNormal() | X.SafeNormal()) < PeripheralVision)
				{
					bShouldDodge = false;
				}
			}
			if (bShouldDodge)
			{
				SetWarningTimer(Incoming, NULL, ProjTime);
			}
		}
		else if (Enemy == NULL)
		{
			SetEnemy(Incoming->Instigator);
		}
	}
}
void AUTBot::ReceiveInstantWarning(AUTCharacter* Shooter, const FVector& FireDir)
{
	if (Shooter != NULL && Shooter->GetWeapon() != NULL && GetPawn() != NULL)
	{
		if (Skill >= 4.0f && Enemy != NULL && FMath::FRand() < 0.2f * (Skill + Personality.Tactics) - 0.33f) // TODO: if 1 on 1 (T)DM be more alert? maybe enemy will usually be set so doesn't matter
		{
			FVector EnemyDir = Shooter->GetActorLocation() - GetPawn()->GetActorLocation();
			EnemyDir.Z = 0;
			FRotationMatrix R(GetControlRotation());
			FVector X = R.GetScaledAxis(EAxis::X);
			X.Z = 0;
			if ((EnemyDir.SafeNormal() | X.SafeNormal()) >= PeripheralVision)
			{
				//LastUnderFire = WorldInfo.TimeSeconds;
				//if (WorldInfo.TimeSeconds - LastWarningTime < 0.5)
				//	return;
				//LastWarningTime = WorldInfo.TimeSeconds;

				const float DodgeTime = Shooter->GetWeapon()->GetRefireTime(Shooter->GetWeapon()->GetCurrentFireMode()) - 0.15 - 0.1 * FMath::FRand(); // TODO: based on collision size 
				if (DodgeTime > 0.0)
				{
					SetWarningTimer(NULL, Shooter, DodgeTime);
				}
			}
		}
		else if (Enemy == NULL)
		{
			SetEnemy(Shooter);
		}
	}
}
void AUTBot::SetWarningTimer(AUTProjectile* Incoming, AUTCharacter* Shooter, float TimeToImpact)
{
	// check that optimal dodge time is far enough in the future for our skills
	if (TimeToImpact >= 0.35f - 0.03f * (Skill + Personality.ReactionTime) && TimeToImpact >= 2.0f - (0.265f + FMath::FRand() * 0.2f) * (Skill + Personality.ReactionTime))
	{
		float WarningDelay;
		if (Skill + Personality.ReactionTime >= 7.0f)
		{
			WarningDelay = FMath::Max<float>(0.08f, FMath::Max<float>(0.35f - 0.025f * (Skill + Personality.ReactionTime) * (1.0f + FMath::FRand()), TimeToImpact - 0.65f));
		}
		else
		{
			WarningDelay = FMath::Max<float>(0.08f, FMath::Max<float>(0.35f - 0.02f * (Skill + Personality.ReactionTime) * (1.0f + FMath::FRand()), TimeToImpact - 0.65f));
		}
		if (!GetWorldTimerManager().IsTimerActive(this, &AUTBot::ProcessIncomingWarning) || WarningDelay < GetWorldTimerManager().GetTimerRate(this, &AUTBot::ProcessIncomingWarning))
		{
			WarningProj = Incoming;
			WarningShooter = (WarningProj != NULL) ? NULL : Shooter;
			// TODO: if in air, consider air control towards wall for wall dodge
			GetWorldTimerManager().SetTimer(this, &AUTBot::ProcessIncomingWarning, WarningDelay, false);
		}
	}
}
void AUTBot::ProcessIncomingWarning()
{
	if (GetUTChar() != NULL && GetUTChar()->UTCharacterMovement != NULL && GetUTChar()->UTCharacterMovement->CanDodge())
	{
		if (WarningProj != NULL)
		{
			if (!WarningProj->bPendingKillPending && !WarningProj->bExploded)
			{
				FVector ProjVel = WarningProj->GetVelocity();
				if (!ProjVel.IsZero())
				{
					FVector Dir = ProjVel.SafeNormal();
					const float TimeToTarget = WarningProj->GetTimeToLocation(GetPawn()->GetActorLocation());
					FVector FuturePos = GetPawn()->GetActorLocation() + GetPawn()->GetVelocity() * TimeToTarget;
					FVector LineDist = FuturePos - (WarningProj->GetActorLocation() + (Dir | (FuturePos - WarningProj->GetActorLocation())) * Dir);
					float Dist = LineDist.Size();
					if (Dist <= 500.0f + GetPawn()->GetSimpleCollisionRadius())
					{
						bool bShouldDodge = true;
						if (Dist > 1.2f * GetPawn()->GetSimpleCollisionHalfHeight())
						{
							if (WarningProj->DamageParams.BaseDamage <= 40 && GetUTChar()->Health > WarningProj->DamageParams.BaseDamage)
							{
								// probably will miss and even if not we can take the hit
								bShouldDodge = false;
							}
							else if (Dist > GetPawn()->GetSimpleCollisionHalfHeight() + 100.0f + WarningProj->GetMaxDamageRadius())
							{
								// projectile's natural flight will miss by more than its best explosive range
								// check that it won't hit a wall on the way that could make it still dangerous (enemy shooting at floor, etc)
								FCollisionQueryParams Params(FName(TEXT("ProjWarning")), false, GetPawn());
								Params.AddIgnoredActor(WarningProj);
								bShouldDodge = GetWorld()->LineTraceTest(WarningProj->GetActorLocation(), WarningProj->GetActorLocation() + ProjVel, COLLISION_TRACE_WEAPONNOCHARACTER, Params);
							}
						}

						if (bShouldDodge)
						{
							// dodge away from projectile
							FRotationMatrix R(GetControlRotation());
							FVector X = R.GetScaledAxis(EAxis::X);
							X.Z = 0;

							//if (!TryDuckTowardsMoveTarget(Dir, Y))
							{
								FVector Y = R.GetScaledAxis(EAxis::Y);
								if ((ProjVel | Y) > 0.0f)
								{
									TryEvasiveAction(Y * -1.0f);
								}
								else
								{
									TryEvasiveAction(Y);
								}
							}
						}
					}
				}
			}
		}
		// skip if shooter is no longer focus and bot isn't skilled enough to keep track of prior threat
		else if (WarningShooter != NULL && !WarningShooter->bPendingKillPending && !WarningShooter->IsDead() && (WarningShooter == Enemy || Personality.Tactics >= 0.5f || Skill + Personality.Tactics >= 5.0f) && LineOfSightTo(WarningShooter))
		{
			// dodge perpendicular to shooter
			FVector Dir = (WarningShooter->GetActorLocation() - GetPawn()->GetActorLocation()).SafeNormal();

			FRotationMatrix R(GetControlRotation());
			FVector X = R.GetScaledAxis(EAxis::X);
			X.Z = 0;

			//if (!TryDuckTowardsMoveTarget(Dir, Y))
			{
				FVector Y = R.GetScaledAxis(EAxis::Y);
				if ((Dir | Y) > 0.0f)
				{
					TryEvasiveAction(Y * -1.0f);
				}
				else
				{
					TryEvasiveAction(Y);
				}
			}
		}
	}
	WarningProj = NULL;
	WarningShooter = NULL;
}

bool AUTBot::TryEvasiveAction(FVector DuckDir)
{
	//if (UTVehicle(Pawn) != None)
//		return UTVehicle(Pawn).Dodge(DCLICK_None);
	//if (Pawn.bStationary)
//		return false;
//	if (Stopped())
//		GotoState('TacticalMove');
//	else if (FRand() < 0.6)
//		bChangeStrafe = IsStrafing();


	if (Skill < 3.0f || GetUTChar() == NULL) // TODO: maybe strafe if not UTCharacter?
	{
		return false;
	}
	else  if (GetCharacter()->bIsCrouched || GetCharacter()->CharacterMovement->bWantsToCrouch)
	{
		return false;
	}
	else
	{
		DuckDir.Z = 0;
		DuckDir *= 700.0f;
		FCollisionShape PawnShape = GetCharacter()->CapsuleComponent->GetCollisionShape();
		FVector Start = GetPawn()->GetActorLocation();
		Start.Z += 50.0f;
		FCollisionQueryParams Params(FName(TEXT("TryEvasiveAction")), false, GetPawn());

		FHitResult Hit;
		bool bHit = GetWorld()->SweepSingle(Hit, Start, Start + DuckDir, FQuat::Identity, ECC_Pawn, PawnShape, Params);

		float MinDist = 350.0f;
		float Dist = (Hit.Location - GetPawn()->GetActorLocation()).Size();
		bool bWallHit = false;
		bool bSuccess = false;
		FVector WallHitLoc;
		if (!bHit || Dist > MinDist)
		{
			if (!bHit)
			{
				Hit.Location = Start + DuckDir;
			}
			bHit = GetWorld()->SweepSingle(Hit, Hit.Location, Hit.Location - FVector(0.0f, 0.0f, 2.5f * GetCharacter()->CharacterMovement->MaxStepHeight), FQuat::Identity, ECC_Pawn, PawnShape, Params);
			bSuccess = (bHit && Hit.Normal.Z >= 0.7);
		}
		else
		{
			bWallHit = (Skill + 2.0f * Personality.Jumpiness) > 5;
			WallHitLoc = Hit.Location + (-Hit.Normal) * 5.0f; // slightly into wall for air controlling into wall dodge
			MinDist = 70.0f + MinDist - Dist;
		}

		if (!bSuccess)
		{
			DuckDir *= -1.0f;
			bHit = GetWorld()->SweepSingle(Hit, Start, Start + DuckDir, FQuat::Identity, ECC_Pawn, PawnShape, Params);
			bSuccess = (!bHit || (Hit.Location - GetPawn()->GetActorLocation()).Size() > MinDist);
			if (bSuccess)
			{
				if (!bHit)
				{
					Hit.Location = Start + DuckDir;
				}

				bHit = GetWorld()->SweepSingle(Hit, Hit.Location, Hit.Location - FVector(0.0f, 0.0f, 2.5f * GetCharacter()->CharacterMovement->MaxStepHeight), FQuat::Identity, ECC_Pawn, PawnShape, Params);
				bSuccess = (bHit && Hit.Normal.Z >= 0.7);
			}
		}
		if (!bSuccess)
		{
			//if (bChangeStrafe)
			//	ChangeStrafe();
			return false;
		}

		if (bWallHit && GetCharacter()->CharacterMovement->MovementMode == MOVE_Falling && Skill + 2.0f * Personality.Jumpiness > 3.0f + 3.0f * FMath::FRand())
		{
			bPlannedWallDodge = true;
			return true;
		}
		else if ( bWallHit && Personality.Jumpiness > 0.0f && GetCharacter()->CanJump() && GetCharacter()->CharacterMovement->MovementMode == MOVE_Walking &&
				(GetCharacter()->CharacterMovement->Velocity.Size() < 0.1f * GetCharacter()->CharacterMovement->MaxWalkSpeed || (GetCharacter()->CharacterMovement->Velocity.SafeNormal2D() | (WallHitLoc - Start).SafeNormal2D()) >= 0.0f) &&
				FMath::FRand() < Personality.Jumpiness * 0.5f )
		{
			// jump towards wall for wall dodge
			GetCharacter()->CharacterMovement->Velocity = (GetCharacter()->CharacterMovement->Velocity + (WallHitLoc - Start)).ClampMaxSize(GetCharacter()->CharacterMovement->MaxWalkSpeed);
			GetCharacter()->CharacterMovement->DoJump(false);
			MoveTargetPoints.Insert(FComponentBasedPosition(WallHitLoc), 0);
			bPlannedWallDodge = true;
			return true;
		}
		else
		{
			//bInDodgeMove = true;
			//DodgeLandZ = GetPawn()->GetActorLocation().Z;
			DuckDir.Normalize();
			GetUTChar()->Dodge(DuckDir, (DuckDir ^ FVector(0.0f, 0.0f, 1.0f)).SafeNormal());
			return true;
		}
	}
}

void AUTBot::SetEnemy(APawn* NewEnemy)
{
	if (NewEnemy != Enemy)
	{
		if (Target == Enemy)
		{
			Target = NULL;
		}
		Enemy = NewEnemy;
		AUTCharacter* EnemyP = Cast<AUTCharacter>(Enemy);
		if (EnemyP != NULL)
		{
			if (EnemyP->IsDead())
			{
				UE_LOG(UT, Warning, TEXT("Bot got dead enemy %s"), *EnemyP->GetName());
				Enemy = NULL;
			}
			else
			{
				EnemyP->MaxSavedPositionAge = FMath::Max<float>(EnemyP->MaxSavedPositionAge, TrackingReactionTime);
			}
		}
		LastEnemyChangeTime = GetWorld()->TimeSeconds;
		if (!bExecutingWhatToDoNext)
		{
			WhatToDoNext();
		}
	}
}

void AUTBot::SetTarget(AActor* NewTarget)
{
	Target = NewTarget;
}

void AUTBot::SeePawn(APawn* Other)
{
	if (Enemy == NULL || !LineOfSightTo(Enemy))
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(Other, this))
		{
			SetEnemy(Other);
			WhatToDoNext();
		}
	}
}

bool AUTBot::CanSee(APawn* Other, bool bMaySkipChecks)
{
	if (Other == NULL || GetPawn() == NULL || (Other->IsA(AUTCharacter::StaticClass()) && false)) // FIXME: IsInvisible()
	{
		return false;
	}
	else if (Other == Enemy)
	{
		// TODO: should still check basic FOV, shouldn't we?
		return LineOfSightTo(Other);
	}
	else
	{
		bLOSflag = !bLOSflag;

		const FVector MyLoc = GetPawn()->GetActorLocation();
		const FVector OtherLoc = Other->GetActorLocation();

		// fixed max sight distance
		if ((OtherLoc - MyLoc).SizeSquared() > FMath::Square(SightRadius))
		{
			return false;
		}
		else
		{
			float Dist = (OtherLoc - MyLoc).Size();

			// may skip if more than 1/5 of maxdist away (longer time to acquire)
			// TODO: check if Other has recently been seen already?
			if (bMaySkipChecks && FMath::FRand() * Dist > 0.1f * SightRadius)
			{
				return false;
			}
			else
			{
				// check field of view
				FVector SightDir = (OtherLoc - MyLoc).SafeNormal();
				const FVector LookDir = GetPawn()->GetViewRotation().Vector();
				if ((SightDir | LookDir) < PeripheralVision)
				{
					return false;
				}
				else if (bMaySkipChecks && bSlowerZAcquire && FMath::FRand() * Dist > 0.1f * SightRadius)
				{
					// lower FOV vertically
					SightDir.Z *= 2.f;
					SightDir.Normalize();
					if ((SightDir | LookDir) < PeripheralVision)
					{
						return false;
					}
					else
					{
						// notice other pawns at very different heights more slowly
						float HeightMod = FMath::Abs(OtherLoc.Z - MyLoc.Z);
						if (FMath::FRand() * Dist < HeightMod)
						{
							return false;
						}
						else
						{
							return Super::LineOfSightTo(Other, FVector(ForceInit), bMaySkipChecks);
						}
					}
				}
				else
				{
					return LineOfSightTo(Other, FVector(ForceInit), bMaySkipChecks);
				}
			}
		}
	}
}
bool AUTBot::LineOfSightTo(const class AActor* Other, FVector ViewPoint, bool bAlternateChecks) const
{
	if (Other == NULL)
	{
		return false;
	}
	else
	{
		if (ViewPoint.IsZero())
		{
			AActor*	ViewTarg = GetViewTarget();
			ViewPoint = ViewTarg->GetActorLocation();
			if (ViewTarg == GetPawn())
			{
				ViewPoint.Z += GetPawn()->BaseEyeHeight; //look from eyes
			}
		}

		static FName NAME_LineOfSight = FName(TEXT("LineOfSight"));
		FVector TargetLocation = Other->GetTargetLocation(GetPawn());

		FCollisionQueryParams CollisionParams(NAME_LineOfSight, true, GetPawn());
		CollisionParams.AddIgnoredActor(Other);

		bool bHit = GetWorld()->LineTraceTest(ViewPoint, TargetLocation, ECC_Visibility, CollisionParams);
		if (Other == Enemy)
		{
			if (bHit)
			{
				bHit = GetWorld()->LineTraceTest(ViewPoint, Enemy->GetActorLocation() + FVector(0.0f, 0.0f, Enemy->BaseEyeHeight), ECC_Visibility, CollisionParams);
			}
			if (!bHit)
			{
				//UpdateEnemyInfo(Enemy);
				return true;
			}
			// only check sides if width of other is significant compared to distance
			else if (FMath::Square(Other->GetSimpleCollisionRadius()) / (Other->GetActorLocation() - ViewPoint).SizeSquared() * 0.0001f)
			{
				return false;
			}
		}
		else if (!bHit)
		{
			return true;
		}
		else
		{
			float DistSq = (Other->GetActorLocation() - ViewPoint).SizeSquared();
			if (DistSq > FMath::Square(SightRadius))
			{
				return false;
			}
			bool bTargetIsPawn = Cast<APawn>(Other) != NULL;
			if (!bTargetIsPawn && Cast<UCapsuleComponent>(Other->GetRootComponent()) == NULL)
			{
				return false;
			}
			// lower distance requirement for advanced checks for monsters or against non-Pawns
			if ((PlayerState == NULL || !bTargetIsPawn) && DistSq > FMath::Square(SightRadius * 0.25f))
			{
				return false;
			}
			// try viewpoint to head
			if ((!bAlternateChecks || !bLOSflag) && !GetWorld()->LineTraceTest(ViewPoint, TargetLocation + FVector(0.0f, 0.0f, Other->GetSimpleCollisionHalfHeight()), ECC_Visibility, CollisionParams))
			{
				return true;
			}
			if (bAlternateChecks && !bLOSflag)
			{
				return false;
			}
			if (FMath::Square(Other->GetSimpleCollisionRadius()) / DistSq < 0.00015f)
			{
				return false;
			}
		}

		//try checking sides - look at dist to four side points, and cull furthest and closest
		FVector Points[4];
		const FVector OtherLoc = Other->GetActorLocation();
		const float OtherRadius = Other->GetSimpleCollisionRadius();
		Points[0] = OtherLoc - FVector(OtherRadius, -1 * OtherRadius, 0);
		Points[1] = OtherLoc + FVector(OtherRadius, OtherRadius, 0);
		Points[2] = OtherLoc - FVector(OtherRadius, OtherRadius, 0);
		Points[3] = OtherLoc + FVector(OtherRadius, -1 * OtherRadius, 0);
		int32 imin = 0;
		int32 imax = 0;
		float currentmin = Points[0].SizeSquared();
		float currentmax = currentmin;
		for (int32 i = 1; i < 4; i++)
		{
			float nextsize = Points[i].SizeSquared();
			if (nextsize > currentmax)
			{
				currentmax = nextsize;
				imax = i;
			}
			else if (nextsize < currentmin)
			{
				currentmin = nextsize;
				imin = i;
			}
		}

		for (int32 i = 0; i < 4; i++)
		{
			if (i != imin && i != imax && !GetWorld()->LineTraceTest(ViewPoint, Points[i], ECC_Visibility, CollisionParams))
			{
				return true;
			}
		}
		return false;
	}
}