// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCharacterMovement.h"
#include "UTBot.h"
#include "UTAIAction.h"
#include "UTAIAction_WaitForMove.h"

AUTBot::AUTBot(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bWantsPlayerState = true;
	RotationRate = FRotator(300.0f, 300.0f, 0.0f);
	SightRadius = 20000.0f;
	PeripheralVision = 0.7f;

	WaitForMoveAction = PCIP.CreateDefaultSubobject<UUTAIAction_WaitForMove>(this, FName(TEXT("WaitForMove")));
}

float FBestInventoryEval::Eval(APawn* Asker, const FNavAgentProperties& AgentProps, const UUTPathNode* Node, const FVector& EntryLoc, int32 TotalDistance)
{
	for (TWeakObjectPtr<AActor> TestActor : Node->POIs)
	{
		if (TestActor.IsValid())
		{
			AUTPickup* TestPickup = Cast<AUTPickup>(TestActor.Get());
			if (TestPickup != NULL && TestPickup->State.bActive) // TODO: flag for pickup timing
			{
				float NewWeight = TestPickup->BotDesireability(Asker, TotalDistance) / FMath::Max<float>(1.0f, float(TotalDistance) + (TestPickup->GetActorLocation() - EntryLoc).Size());
				if (NewWeight > BestWeight)
				{
					BestPickup = TestPickup;
					BestWeight = NewWeight;
				}
				return NewWeight;
			}
		}
	}
	return 0.0f;
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

	MoveTarget.Clear();
	RouteCache.Empty();
	MoveTargetPoints.Empty();
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
				if (MoveTimer < 0.0f)
				{
					// timed out
					ClearMoveTarget();
				}
				else
				{
					// TODO: need some sort of failure checks in here

					// clear points that we've reached or passed
					FVector MyLoc = MyPawn->GetActorLocation();
					float DistFromTarget = (MoveTarget.GetLocation() - MyLoc).Size();
					FVector Extent = MyPawn->GetSimpleCollisionCylinderExtent();
					for (int32 i = MoveTargetPoints.Num() - 2; i >= 0; i--)
					{
						FBox TestBox(0);
						TestBox += MyLoc + Extent;
						TestBox += MyLoc - Extent;
						if (TestBox.IsInside(MoveTargetPoints[i]))
						{
							MoveTargetPoints.RemoveAt(0, i + 1);
							break;
						}
						// if path requires complex movement (jumps, etc) then require touching second to last point
						// since the final part of the path may require more precision
						if (i < MoveTargetPoints.Num() - 2 || CurrentPath.ReachFlags == 0)
						{
							FVector HitLoc;
							if (DistFromTarget < (MoveTarget.GetLocation() - MoveTargetPoints[i]).Size() && !NavData->Raycast(MyLoc, MoveTargetPoints[i], HitLoc, NULL))
							{
								MoveTargetPoints.RemoveAt(0, i + 1);
								break;
							}
						}
					}

					// failure checks
					if ((MoveTargetPoints[0] - MyLoc).Size2D() < MyPawn->GetSimpleCollisionRadius() && FMath::Abs<float>(MyLoc.Z - MoveTargetPoints[0].Z) > MyPawn->GetSimpleCollisionHalfHeight())
					{
						if (GetCharacter() != NULL)
						{
							if (GetCharacter()->CharacterMovement->MovementMode == MOVE_Walking)
							{
								// failed - directly above or below target
								ClearMoveTarget();
							}
						}
						else if (GetWorld()->SweepTest(MyLoc, MoveTargetPoints[0], FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(MyPawn->GetSimpleCollisionCylinderExtent() * FVector(0.9f, 0.9f, 0.1f)), FCollisionQueryParams(FName(TEXT("AIZCheck")), false, MyPawn)))
						{
							// failed - directly above or below target
							ClearMoveTarget();
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
				FVector TargetLoc;
				if (MoveTargetPoints.Num() == 1)
				{
					TargetLoc = MoveTarget.GetLocation();
				}
				else
				{
					TargetLoc = MoveTargetPoints[0];
				}
				if (Enemy != NULL && LineOfSightTo(Enemy)) // TODO: cache this
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
						SetFocalPoint(MoveTarget.GetLocation()); // hmm, better in some situations, worse in others. Maybe periodically trace? Or maybe it doesn't matter because we'll have an enemy most of the time
					}
				}
				if (GetCharacter() != NULL && GetCharacter()->CharacterMovement != NULL)
				{
					GetCharacter()->CharacterMovement->bCanWalkOffLedges = CurrentPath.ReachFlags & R_JUMP;
				}
				if (GetCharacter()->CharacterMovement->MovementMode == MOVE_Falling && GetCharacter()->CharacterMovement->AirControl > 0.0f)
				{
					// make sure we don't overshoot with air control
					FVector DesiredDir = TargetLoc - MyPawn->GetActorLocation();
					DesiredDir.Z = 0.0f;
					float Pct = FMath::Min<float>(1.0f, DesiredDir.Size() / FMath::Max<float>(1.0f, GetCharacter()->CharacterMovement->MaxWalkSpeed * DeltaTime / GetCharacter()->CharacterMovement->AirControl));
					MyPawn->GetMovementComponent()->AddInputVector(DesiredDir.SafeNormal2D() * Pct);
				}
				else
				{
					MyPawn->GetMovementComponent()->AddInputVector((TargetLoc - MyPawn->GetActorLocation()).SafeNormal2D());
				}
			}
		}
	}
}

void AUTBot::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	if (GetPawn() != NULL)
	{
		if (MoveTarget.IsValid())
		{
			for (int32 i = 0; i < MoveTargetPoints.Num(); i++)
			{
				DrawDebugLine(GetWorld(), (i == 0) ? GetPawn()->GetActorLocation() : MoveTargetPoints[i - 1], MoveTargetPoints[i], FColor(0, 255, 0));
			}
		}
		for (const FRouteCacheItem& RoutePoint : RouteCache)
		{
			DrawDebugSphere(GetWorld(), RoutePoint.GetLocation(), 16.0f, 8, FColor(0, 255, 0));
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
			FVector Direction = FocalPoint - P->GetActorLocation();
			FRotator DesiredRotation = Direction.Rotation();

			// Don't pitch view of walking pawns unless looking at another pawn
			if (GetPawn()->GetMovementComponent() && GetPawn()->GetMovementComponent()->IsMovingOnGround() && Cast<APawn>(GetFocusActor()) == NULL)
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
		FVector Diff = MoveTarget.GetLocation() - GetCharacter()->GetActorLocation();
		float XYTime = Diff.Size2D() / GetCharacter()->CharacterMovement->MaxWalkSpeed;
		float DesiredJumpZ = Diff.Z / XYTime - 0.5f * GetCharacter()->CharacterMovement->GetGravityZ() * XYTime;
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
				if (DodgeDesiredJumpZ <= GetUTChar()->UTCharacterMovement->DodgeImpulseVertical && !GetWorld()->LineTraceTest(GetCharacter()->GetActorLocation(), MoveTarget.GetLocation() + FVector(0.0f, 0.0f, 60.0f), ECC_Pawn, TraceParams))
				{
					FRotationMatrix YawMat(FRotator(0.f, GetUTChar()->GetActorRotation().Yaw, 0.f));
					// forward dodge
					FVector X = YawMat.GetScaledAxis(EAxis::X).SafeNormal();
					FVector Y = YawMat.GetScaledAxis(EAxis::Y).SafeNormal();
					GetUTChar()->UTCharacterMovement->PerformDodge(X, Y);
					bDodged = true;
				}
			}
			if (!bDodged)
			{
				GetCharacter()->CharacterMovement->Velocity = Diff.SafeNormal2D() * FMath::Min<float>(GetCharacter()->CharacterMovement->MaxWalkSpeed, DesiredJumpZ / FMath::Max<float>(GetCharacter()->CharacterMovement->JumpZVelocity, 1.0f));
				GetCharacter()->CharacterMovement->DoJump(false);
			}
		}
		else
		{
			// clamp initial XY speed if target is directly below
			float ZTime = FMath::Sqrt(Diff.Z / (0.5f * GetCharacter()->CharacterMovement->GetGravityZ()));
			GetCharacter()->CharacterMovement->Velocity = GetCharacter()->CharacterMovement->Velocity.SafeNormal() * FMath::Min<float>(GetCharacter()->CharacterMovement->Velocity.Size2D(), Diff.Size2D() / ZTime);
		}
	}
}

void AUTBot::NotifyMoveBlocked(const FHitResult& Impact)
{
	if (GetCharacter() != NULL)
	{
		if (GetCharacter()->CharacterMovement->MovementMode == MOVE_Walking)
		{
			if (MoveTarget.IsValid() && (CurrentPath.ReachFlags & R_JUMP)) // TODO: maybe also if chasing enemy?)
			{
				FVector Diff = MoveTarget.GetLocation() - GetCharacter()->GetActorLocation();
				float XYTime = Diff.Size2D() / GetCharacter()->CharacterMovement->MaxWalkSpeed;
				float DesiredJumpZ = Diff.Z / XYTime - 0.5 * GetCharacter()->CharacterMovement->GetGravityZ() * XYTime;
				if (DesiredJumpZ > 0.0f)
				{
					GetCharacter()->CharacterMovement->Velocity = GetCharacter()->CharacterMovement->Velocity.SafeNormal() * FMath::Min<float>(GetCharacter()->CharacterMovement->MaxWalkSpeed, DesiredJumpZ / FMath::Max<float>(GetCharacter()->CharacterMovement->JumpZVelocity, 1.0f));
				}
				GetCharacter()->CharacterMovement->DoJump(false);
			}
			else if (Impact.Time <= 0.0f)
			{
				ClearMoveTarget();
			}
		}
		else if (GetCharacter()->CharacterMovement->MovementMode == MOVE_Falling)
		{
			// TODO: maybe wall dodge
		}
	}
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

		for (AUTInventory* Inv = GetUTChar()->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
		{
			AUTWeapon* TestWeap = Cast<AUTWeapon>(Inv);
			if (TestWeap != NULL)
			{
				float NewRating = RateWeapon(TestWeap);
				if (NewRating > BestRating)
				{
					Best = TestWeap;
					BestRating = NewRating;
				}
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

void AUTBot::WhatToDoNext()
{
	ensure(!bExecutingWhatToDoNext);

	bPendingWhatToDoNext = true;
}

void AUTBot::ExecuteWhatToDoNext()
{
	SwitchToBestWeapon();

	if (Enemy != NULL && GetUTChar() != NULL && GetUTChar()->GetWeapon() != NULL && GetUTChar()->GetWeapon()->BaseAISelectRating >= 0.5f && LineOfSightTo(Enemy))
	{
		GetUTChar()->StartFire(0);
		if (Enemy != NULL) // shot may kill enemy
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
	}
	if (CurrentAction == NULL)
	{
		if (GetUTChar() != NULL)
		{
			GetUTChar()->StopFire(0);
		}
		float Weight = 0.0f;
		FBestInventoryEval NodeEval;
		NavData->FindBestPath(GetPawn(), *GetPawn()->GetNavAgentProperties(), NodeEval, GetPawn()->GetNavAgentLocation(), Weight, true, RouteCache);
		if (RouteCache.Num() > 0)
		{
			SetMoveTarget(RouteCache[0]);
			StartNewAction(WaitForMoveAction);
		}
	}
}

void AUTBot::UTNotifyKilled(AController* Killer, AController* KilledPlayer, APawn* KilledPawn, const UDamageType* DamageType)
{
	if (KilledPawn == Enemy)
	{
		Enemy = NULL;
		// TODO: maybe taunt
	}
}

void AUTBot::SeePawn(APawn* Other)
{
	Enemy = Other;
	WhatToDoNext();
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