// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCharacterMovement.h"
#include "UTBot.h"
#include "UTAIAction.h"
#include "UTAIAction_WaitForMove.h"
#include "UTAIAction_WaitForLanding.h"
#include "UTAIAction_TacticalMove.h"
#include "UTAIAction_RangedAttack.h"
#include "UTAIAction_Charge.h"
#include "UTDroppedPickup.h"
#include "UTPickupInventory.h"
#include "UTPickupHealth.h"
#include "UTSquadAI.h"

void FBotEnemyInfo::Update(EAIEnemyUpdateType UpdateType, const FVector& ViewerLoc)
{
	if (Pawn != NULL)
	{
		// if we haven't received full info on this enemy in a while, assume our health estimate is off
		if (Pawn->GetWorld()->TimeSeconds - LastFullUpdateTime > 10.0f)
		{
			bHasExactHealth = false;
		}

		LastUpdateTime = Pawn->GetWorld()->TimeSeconds;
		switch (UpdateType)
		{
		case EUT_Seen:
			LastSeenLoc = Pawn->GetActorLocation();
			LastSeeingLoc = ViewerLoc;
			LastKnownLoc = LastSeenLoc;
			LastSeenTime = Pawn->GetWorld()->TimeSeconds;
			LastFullUpdateTime = Pawn->GetWorld()->TimeSeconds;
			bLostEnemy = false;
			if (!bHasExactHealth && UTChar != NULL)
			{
				EffectiveHealthPct = UTChar->GetEffectiveHealthPct(true);
			}
			break;
		case EUT_HeardExact:
			LastKnownLoc = Pawn->GetActorLocation();
			LastFullUpdateTime = Pawn->GetWorld()->TimeSeconds;
			bLostEnemy = false;
			break;
		case EUT_HeardApprox:
			// TODO: set a "general area" sphere?
			break;
		case EUT_TookDamage:
			LastHitByTime = Pawn->GetWorld()->TimeSeconds;
			// TODO: only update LastKnownLoc/LastFullUpdateTime if recently fired the projectile?
			LastKnownLoc = Pawn->GetActorLocation();
			LastFullUpdateTime = Pawn->GetWorld()->TimeSeconds;
			bLostEnemy = false;
			break;
		case EUT_DealtDamage:
			// TODO: only update LastKnownLoc if recently fired the projectile?
			LastKnownLoc = Pawn->GetActorLocation();
			LastFullUpdateTime = Pawn->GetWorld()->TimeSeconds;
			bLostEnemy = false;
			if (UTChar != NULL)
			{
				EffectiveHealthPct = UTChar->GetEffectiveHealthPct(false);
				bHasExactHealth = true;
			}
			break;
		case EUT_HealthUpdate:
			LastKnownLoc = Pawn->GetActorLocation();
			LastFullUpdateTime = Pawn->GetWorld()->TimeSeconds;
			bLostEnemy = false;
			if (UTChar != NULL && bHasExactHealth)
			{
				EffectiveHealthPct = UTChar->GetEffectiveHealthPct(false);
			}
			break;
		}
	}
}
bool FBotEnemyInfo::IsValid(AActor* TeamHolder) const
{
	if (Pawn == NULL || Pawn->bPendingKillPending || (UTChar != NULL && UTChar->IsDead()))
	{
		return false;
	}
	else if (TeamHolder == NULL)
	{
		return true;
	}
	else
	{
		AUTGameState* GS = TeamHolder->GetWorld()->GetGameState<AUTGameState>();
		return (GS == NULL || !GS->OnSameTeam(TeamHolder, Pawn));
	}
}

AUTBot::AUTBot(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	TacticalHeightAdvantage = 650.0f;

	bWantsPlayerState = true;
	SightRadius = 20000.0f;
	RotationRate = FRotator(300.0f, 300.0f, 0.0f);
	PeripheralVision = 0.7f;
	TrackingReactionTime = 0.25f;
	LastIterativeLeadCheck = 1.0f;
	TacticalAimUpdateInterval = 0.2f;

	WaitForMoveAction = PCIP.CreateDefaultSubobject<UUTAIAction_WaitForMove>(this, FName(TEXT("WaitForMove")));
	WaitForLandingAction = PCIP.CreateDefaultSubobject<UUTAIAction_WaitForLanding>(this, FName(TEXT("WaitForLanding")));
	TacticalMoveAction = PCIP.CreateDefaultSubobject<UUTAIAction_TacticalMove>(this, FName(TEXT("TacticalMove")));
	RangedAttackAction = PCIP.CreateDefaultSubobject<UUTAIAction_RangedAttack>(this, FName(TEXT("RangedAttack")));
	ChargeAction = PCIP.CreateDefaultSubobject<UUTAIAction_Charge>(this, FName(TEXT("Charge")));
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

void AUTBot::InitializeSkill(float NewBaseSkill)
{
	Skill = NewBaseSkill + Personality.SkillModifier;

	if (Skill >= 0)
	{
		TrackingReactionTime = GetClass()->GetDefaultObject<AUTBot>()->TrackingReactionTime * 7.0f / (Skill + 2.0f);
	}

	/*if (Skill < 3)
		DodgeToGoalPct = 0;
	else
		DodgeToGoalPct = (Jumpiness * 0.5) + Skill / 6;*/

	bLeadTarget = Skill >= 4.0f;
	SetPeripheralVision();
	HearingRadiusMult = FMath::Clamp<float>(Skill / 6.5f, 0.0f, 0.9f);

	if (Skill + Personality.ReactionTime >= 7.0f)
	{
		RotationRate.Yaw = 604.0f;
	}
	else if (Skill + Personality.ReactionTime >= 4.0f)
	{
		RotationRate.Yaw = 38.5f + 55.0f * (Skill + Personality.ReactionTime);
	}
	else
	{
		RotationRate.Yaw = 164.0f + 22.0f * (Skill + Personality.ReactionTime);
	}
	RotationRate.Pitch = RotationRate.Yaw;
	//AdjustedYaw = (0.75 + 0.05 * ReactionTime) * RotationRate.Yaw;
	//AcquisitionYawRate = AdjustedYaw;
	//SetMaxDesiredSpeed();
}

void AUTBot::SetPeripheralVision()
{
	if (GetPawn() != NULL)
	{
		/*if (Pawn.bStationary || (Pawn.Physics == PHYS_Flying))
		{
			bSlowerZAcquire = false;
			Pawn.PeripheralVision = -0.7;
		}
		else
		*/
		{
			if (Skill < 2.0f)
			{
				PeripheralVision = 0.7f;
				bSlowerZAcquire = true;
			}
			else if (Skill > 6.0f)
			{
				bSlowerZAcquire = false;
				PeripheralVision = 0.0f;
			}
			else
			{
				PeripheralVision = 1.0f - 0.2f * Skill;
			}

			PeripheralVision = FMath::Min<float>(PeripheralVision - Personality.Alertness * 0.5f, 0.8f);
		}
	}
}

void AUTBot::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	UTChar = Cast<AUTCharacter>(GetPawn());

	SetPeripheralVision();
}

void AUTBot::Possess(APawn* InPawn)
{
	Super::Possess(InPawn);

	ClearMoveTarget();
	bPickNewFireMode = true;
	
	// set weapon timer, if not already
	GetWorldTimerManager().SetTimer(this, &AUTBot::CheckWeaponFiringTimed, 1.2f - 0.09f * FMath::Min<float>(10.0f, Skill + Personality.ReactionTime), true);
}

void AUTBot::PawnPendingDestroy(APawn* InPawn)
{
	Enemy = NULL;
	StartNewAction(NULL);

	Super::PawnPendingDestroy(InPawn);
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
	
	SetSquad(NULL);

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
		// all bots need a SquadAI, even if it's a squad of one
		if (Squad == NULL)
		{
			AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if (Game != NULL)
			{
				Game->AssignDefaultSquadFor(this);
			}
			if (Squad == NULL)
			{
				UE_LOG(UT, Warning, TEXT("Bot %s failed to get Squad from game mode!"), (PlayerState != NULL) ? *PlayerState->PlayerName : *GetName());
				// force default so we always have one
				SetSquad(GetWorld()->SpawnActor<AUTSquadAI>());
			}
		}

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
		// check current enemy every frame, others on a slightly random timer to avoid hitches
		if (Enemy != NULL)
		{
			if (CanSee(Enemy, false))
			{
				SeePawn(Enemy);
			}
			else if (CurrentAction != NULL)
			{
				CurrentAction->EnemyNotVisible();
			}
		}
		SightCounter -= DeltaTime;
		if (SightCounter < 0.0f)
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
			{
				if (It->IsValid())
				{
					AController* C = It->Get();
					if (C != this && C->GetPawn() != NULL && C->GetPawn() != Enemy && (bSeeFriendly || GS == NULL || !GS->OnSameTeam(C, this)) && CanSee(C->GetPawn(), true) && (!AreAIIgnoringPlayers() || !C->IsA(APlayerController::StaticClass())))
					{
						SeePawn(C->GetPawn());
					}
				}
			}
			SightCounter += 0.15f + 0.1f * FMath::SRand();
		}

		// process current action
		{
			UUTAIAction* SavedAction = CurrentAction;
			if (SavedAction == NULL)
			{
				WhatToDoNext();
			}
			else if (SavedAction->Update(DeltaTime))
			{
				if (SavedAction == CurrentAction) // could have ended itself...
				{
					CurrentAction->Ended(false);
					CurrentAction = NULL;
					WhatToDoNext();
				}
				else if (SavedAction == NULL) // could also interrupt directly to another action
				{
					WhatToDoNext();
				}
			}
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
				if (MoveTarget.IsDirectTarget())
				{
					// a little hacky... this location isn't actually used as the below movement code always moves to MoveTarget directly when on the last point
					MoveTargetPoints.Add(FComponentBasedPosition(MoveTarget.GetLocation(GetPawn())));
				}
				else
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
					GetCharacter()->CharacterMovement->bCanWalkOffLedges = (!CurrentPath.IsSet() || (CurrentPath.ReachFlags & R_JUMP)) && (CurrentAction == NULL || CurrentAction->AllowWalkOffLedges());
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

	Canvas->SetDrawColor(0, 255, 0);
	Canvas->DrawText(GEngine->GetSmallFont(), GoalString, 4.0f, YPos);
	YPos += YL;
	YPos += YL;
	Canvas->SetDrawColor(255, 0, 0);
	Canvas->DrawText(GEngine->GetSmallFont(), FString::Printf(TEXT("ENEMIES (current: %s, last rated %f ago)"), (Enemy != NULL && Enemy->PlayerState != NULL) ? *Enemy->PlayerState->PlayerName : *GetNameSafe(Enemy), GetWorld()->TimeSeconds - LastPickEnemyTime), 4.0f, YPos);
	YPos += YL;
	for (const FBotEnemyRating& RatingInfo : LastPickEnemyRatings)
	{
		Canvas->DrawText(GEngine->GetSmallFont(), FString::Printf(TEXT("%s rated %4.2f"), *RatingInfo.PlayerName, RatingInfo.Rating), 4.0f, YPos);
		YPos += YL;
	}
	YPos += YL;

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

void AUTBot::ApplyWeaponAimAdjust(FVector TargetLoc, FVector& FocalPoint)
{
	AUTWeapon* MyWeap = (UTChar != NULL) ? UTChar->GetWeapon() : NULL;
	if (MyWeap != NULL)
	{
		// tactical aim adjustments
		// if weapon returns a custom aim target it's responsible for this part (including leading, if desired)
		bool bInstantHit = true;
		if (TargetLoc == FocalPoint)
		{
			AUTProjectile* DefaultProj = NULL;
			if (MyWeap->ProjClass.IsValidIndex(NextFireMode) && MyWeap->ProjClass[NextFireMode] != NULL)
			{
				DefaultProj = MyWeap->ProjClass[NextFireMode].GetDefaultObject();
				// handle leading
				if (bLeadTarget)
				{
					float TravelTime = DefaultProj->GetTimeToLocation(FocalPoint);
					if (TravelTime > 0.0f)
					{
						FVector FireLocation = GetPawn()->GetActorLocation();
						FireLocation.Z += GetPawn()->BaseEyeHeight;
						bInstantHit = false;

						ACharacter* EnemyChar = Cast<ACharacter>(GetTarget());
						if (EnemyChar != NULL && EnemyChar->CharacterMovement->MovementMode == MOVE_Falling)
						{
							// take gravity and landing into account
							TrackedVelocity.Z = TrackedVelocity.Z + 0.5f * TravelTime * EnemyChar->CharacterMovement->GetGravityZ();
						}
						FocalPoint += TrackedVelocity * TravelTime;

						if (BlockedAimTarget == GetTarget() || GetWorld()->TimeSeconds - LastTacticalAimUpdateTime < TacticalAimUpdateInterval)
						{
							BlockedAimTarget = NULL;

							// make sure enemy is not hemmed by wall (or landing)
							FCollisionQueryParams Params(FName(TEXT("AimWallCheck")), false, GetPawn());
							Params.AddIgnoredActor(GetTarget());
							FHitResult Hit;
							if (GetWorld()->LineTraceSingle(Hit, (GetTarget() == Enemy) ? GetEnemyLocation(Enemy, false) : GetTarget()->GetActorLocation(), FocalPoint, COLLISION_TRACE_WEAPON, Params))
							{
								BlockedAimTarget = GetTarget();
								FocalPoint = Hit.Location - 24.f * TrackedVelocity.SafeNormal();
							}

							// make sure have a clean shot at where I think he's going
							FVector ProjStart = GetPawn()->GetActorLocation();
							ProjStart.Z += GetPawn()->BaseEyeHeight;
							ProjStart += (FocalPoint - ProjStart).SafeNormal() * MyWeap->FireOffset.X;
							if (GetWorld()->LineTraceSingle(Hit, ProjStart, FocalPoint, COLLISION_TRACE_WEAPON, Params))
							{
								BlockedAimTarget = GetTarget();
								// apply previous iterative value
								FocalPoint = TargetLoc + LastIterativeLeadCheck * (FocalPoint - TargetLoc);
								if (GetWorld()->LineTraceSingle(Hit, ProjStart, FocalPoint, COLLISION_TRACE_WEAPON, Params))
								{
									// see if head would work
									FocalPoint.Z += Enemy->BaseEyeHeight;

									if (GetWorld()->LineTraceSingle(Hit, ProjStart, FocalPoint, COLLISION_TRACE_WEAPON, Params))
									{
										// iteratively track down correct aim spot over multiple ticks
										LastIterativeLeadCheck *= 0.5f;
									}
									else
									{
										LastIterativeLeadCheck = FMath::Clamp<float>(LastIterativeLeadCheck + 0.5f * (1.f - LastIterativeLeadCheck), 0.f, 1.f);
									}
								}
								else
								{
									LastIterativeLeadCheck = FMath::Clamp<float>(LastIterativeLeadCheck + 0.5f * (1.f - LastIterativeLeadCheck), 0.f, 1.f);
								}
								FocalPoint = TargetLoc + LastIterativeLeadCheck * (FocalPoint - TargetLoc);
							}
							else
							{
								LastIterativeLeadCheck = 1.f;
							}
						}
					}
				}
			}

			if (GetWorld()->TimeSeconds - LastTacticalAimUpdateTime < TacticalAimUpdateInterval)
			{
				FocalPoint += TacticalAimOffset;
			}
			else
			{
				LastTacticalAimUpdateTime = GetWorld()->TimeSeconds;;

				TargetLoc = FocalPoint;
				const FVector FireStart = MyWeap->GetFireStartLoc();

				// handle tossed projectiles
				if (DefaultProj != NULL && DefaultProj->ProjectileMovement != NULL && DefaultProj->ProjectileMovement->ProjectileGravityScale > 0.0f)
				{
					// TODO: calculate toss direction, set FocalPoint to toss dir
				}
				else
				{
					FCollisionQueryParams Params(FName(TEXT("ApplyWeaponAimAdjust")), false, GetPawn());
					Params.AddIgnoredActor(GetFocusActor());
					FCollisionObjectQueryParams ResultParams(ECC_WorldStatic);
					ResultParams.AddObjectTypesToQuery(ECC_WorldDynamic);

					const float TargetHeight = GetFocusActor()->GetSimpleCollisionHalfHeight();
					AUTCharacter* EnemyChar = Cast<AUTCharacter>(GetFocusActor());
					bool bDefendMelee = (EnemyChar != NULL && EnemyChar->GetWeapon() != NULL && EnemyChar->GetWeapon()->bMeleeWeapon);

					bool bClean = false; // so will fail first check unless shooting at feet

					if ( MyWeap->bRecommendSplashDamage && EnemyChar != NULL && (Skill >= 4.0f + FMath::Max<float>(0.0f, Personality.Accuracy + Personality.Tactics) || bDefendMelee)
						&& ( (EnemyChar->CharacterMovement->MovementMode == MOVE_Falling && (GetPawn()->GetActorLocation().Z + 180.0f >= FocalPoint.Z))
							|| (GetPawn()->GetActorLocation().Z + 40.0f >= FocalPoint.Z && (bDefendMelee || Skill > 6.5f * FMath::FRand() - 0.5f)) ) )
					{
						FHitResult Hit;
						bClean = !GetWorld()->LineTraceSingle(Hit, TargetLoc, TargetLoc - FVector(0.0f, 0.0f, TargetHeight + 13.0f), Params, ResultParams);
						if (!bClean)
						{
							TargetLoc = Hit.Location + FVector(0.0f, 0.0f, 6.0f);
							bClean = !GetWorld()->LineTraceTest(FireStart, TargetLoc, Params, ResultParams);
						}
						else
						{
							bClean = (EnemyChar->CharacterMovement->MovementMode == MOVE_Falling && !GetWorld()->LineTraceTest(FireStart, TargetLoc, Params, ResultParams));
						}
					}
					bool bCheckedHead = false;
					bool bHeadClean = false;
					if (MyWeap->bSniping && ((IsStopped() && Skill + Personality.Accuracy > 5.0f + 6.0f * FMath::FRand()) || (FMath::FRand() < Personality.Accuracy && MyWeap->GetHeadshotScale() > 0.0f)))
					{
						// try head
						TargetLoc.Z = FocalPoint.Z + 0.9f * TargetHeight;
						bClean = !GetWorld()->LineTraceTest(FireStart, FocalPoint, Params, ResultParams);
						bCheckedHead = true;
						bHeadClean = bClean;
					}

					if (!bClean)
					{
						// try middle
						TargetLoc.Z = FocalPoint.Z;
						bClean = !GetWorld()->LineTraceTest(FireStart, FocalPoint, Params, ResultParams);
					}

					if (!bClean)
					{
						// try head
						TargetLoc.Z = FocalPoint.Z + 0.9f * TargetHeight;
						bClean = bCheckedHead ? bHeadClean : !GetWorld()->LineTraceTest(FireStart, FocalPoint, Params, ResultParams);
					}
					if (!bClean && GetFocusActor() == Enemy)
					{
						const FBotEnemyInfo* EnemyInfo = GetEnemyInfo(Enemy, false);
						if (GetWorld()->TimeSeconds - EnemyInfo->LastSeenTime < 10.0f)
						{
							TargetLoc = EnemyInfo->LastSeenLoc;
							if (GetPawn()->GetActorLocation().Z >= TargetLoc.Z)
							{
								TargetLoc.Z -= 0.4f * TargetHeight;
							}
							FHitResult Hit;
							if (GetWorld()->LineTraceSingle(Hit, FireStart, TargetLoc, Params, ResultParams))
							{
								TargetLoc = EnemyInfo->LastSeenLoc + 2.0f * TargetHeight * Hit.Normal;
								if (MyWeap != NULL && MyWeap->GetDamageRadius(NextFireMode) > 0.0f && Skill >= 4.0f && GetWorld()->LineTraceSingle(Hit, FireStart, TargetLoc, Params, ResultParams))
								{
									TargetLoc += 2.0f * TargetHeight * Hit.Normal;
								}
							}
						}
					}
				}
				TacticalAimOffset = TargetLoc - FocalPoint;
				FocalPoint = TargetLoc;
			}
		}
	}
}

void AUTBot::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	// Look toward focus
	FVector FocalPoint = Super::GetFocalPoint(); // our version returns adjusted result
	if (!FocalPoint.IsZero())
	{
		APawn* P = GetPawn();
		if (P != NULL)
		{
			const float WorldTime = GetWorld()->TimeSeconds;

			if (GetFocusActor() != NULL)
			{
				TrackedVelocity = GetFocusActor()->GetVelocity();
			}

			// warning: assumption that if bot wants to shoot an enemy Pawn it always sets it as Enemy
			if (GetFocusActor() == Enemy)
			{
				const TArray<FSavedPosition>* SavedPosPtr = NULL;
				if (IsEnemyVisible(Enemy))
				{
					AUTCharacter* TargetP = Cast<AUTCharacter>(Enemy);
					if (TargetP != NULL && TargetP->SavedPositions.Num() > 0 && TargetP->SavedPositions[0].Time <= WorldTime - TrackingReactionTime)
					{
						SavedPosPtr = &TargetP->SavedPositions;
					}
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
							TrackedVelocity = SavedPositions[i - 1].Velocity + (SavedPositions[i].Velocity - SavedPositions[i - 1].Velocity) * (WorldTime - TrackingReactionTime - SavedPositions[i - 1].Time) / (SavedPositions[i].Time - SavedPositions[i - 1].Time);
							TargetLoc = TargetLoc + TrackedVelocity * TrackingReactionTime;

							if (CanAttack(Enemy, TargetLoc, false, !bPickNewFireMode, &NextFireMode, &FocalPoint))
							{
								bPickNewFireMode = false;
								ApplyWeaponAimAdjust(TargetLoc, FocalPoint);
							}
							else
							{
								FocalPoint = TargetLoc;
							}

							break;
						}
					}
				}
				else if (CanAttack(Enemy, GetEnemyLocation(Enemy, true), false, !bPickNewFireMode, &NextFireMode, &FocalPoint))
				{
					bPickNewFireMode = false;
					ApplyWeaponAimAdjust(GetEnemyLocation(Enemy, true), FocalPoint);
				}
				else
				{
					FocalPoint = GetEnemyLocation(Enemy, false);
				}
			}
			else if (Target != NULL && GetFocusActor() == Target)
			{
				FVector TargetLoc = GetFocusActor()->GetTargetLocation();
				if (CanAttack(GetFocusActor(), TargetLoc, false, !bPickNewFireMode, &NextFireMode, &FocalPoint))
				{
					bPickNewFireMode = false;
					ApplyWeaponAimAdjust(TargetLoc, FocalPoint);
				}
			}

			FinalFocalPoint = FocalPoint; // for later GetFocalPoint() queries

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
	if ((CurrentAction == NULL || !CurrentAction->NotifyMoveBlocked(Impact)) && GetCharacter() != NULL)
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
	if (TestClass == NULL)
	{
		return false;
	}
	else
	{
		for (UClass* CurrentClass = *TestClass; CurrentClass != NULL; CurrentClass = CurrentClass->GetSuperClass())
		{
			if (CurrentClass->GetFName() == Personality.FavoriteWeapon)
			{
				return true;
			}
		}
		return false;
	}
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
	else if (UTChar->GetWeapon() != NULL && UTChar->GetPendingWeapon() == NULL && (bFromWeapon || !UTChar->GetWeapon()->IsFiring())) // if weapon is firing, it should query bot when it's done for better responsiveness than a timer
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

				// if blew self up, abort
				if (UTChar == NULL)
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

bool AUTBot::CanAttack(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8* BestFireMode, FVector* OptimalTargetLoc)
{
	if (GetUTChar() == NULL || GetUTChar()->GetWeapon() == NULL)
	{
		return false;
	}
	else
	{
		uint8 TempFireMode = 0;
		FVector TempTargetLoc = FVector::ZeroVector;
		if (BestFireMode == NULL)
		{
			BestFireMode = &TempFireMode;
		}
		if (OptimalTargetLoc == NULL)
		{
			OptimalTargetLoc = &TempTargetLoc;
		}
		return GetUTChar()->GetWeapon()->CanAttack(Target, TargetLoc, bDirectOnly, bPreferCurrentMode, *BestFireMode, *OptimalTargetLoc);
	}
}

bool AUTBot::CheckFutureSight(float DeltaTime)
{
	FVector FutureLoc = GetPawn()->GetActorLocation();
	if (GetCharacter() != NULL)
	{
		if (!GetCharacter()->CharacterMovement->GetCurrentAcceleration().IsZero())
		{
			FutureLoc += GetCharacter()->CharacterMovement->GetMaxSpeed() * DeltaTime * GetCharacter()->CharacterMovement->GetCurrentAcceleration().SafeNormal();
		}

		if (GetCharacter()->GetMovementBase() != NULL)
		{
			FutureLoc += GetCharacter()->GetMovementBase()->GetComponentVelocity() * DeltaTime;
		}
	}
	FCollisionQueryParams Params(FName(TEXT("CheckFutureSight")), false, GetPawn());
	FCollisionObjectQueryParams ResultParams(ECC_WorldStatic);
	ResultParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	//make sure won't run into something
	if (GetCharacter() != NULL && GetCharacter()->CharacterMovement->MovementMode != MOVE_Walking && GetWorld()->LineTraceTest(GetPawn()->GetActorLocation(), FutureLoc, Params, ResultParams))
	{
		return false;
	}
	else
	{
		// check if can still see target
		return !GetWorld()->LineTraceTest(FutureLoc, GetFocalPoint() + TrackedVelocity, Params, ResultParams);
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

const FBotEnemyInfo* AUTBot::GetEnemyInfo(APawn* TestEnemy, bool bCheckTeam)
{
	AUTPlayerState* PS = bCheckTeam ? Cast<AUTPlayerState>(PlayerState) : NULL;
	const TArray<const FBotEnemyInfo>& EnemyList = (PS != NULL && PS->Team != NULL) ? PS->Team->GetEnemyList() : *(const TArray<const FBotEnemyInfo>*)&LocalEnemyList;
	for (int32 i = 0; i < EnemyList.Num(); i++)
	{
		if (EnemyList[i].GetPawn() == TestEnemy)
		{
			return &EnemyList[i];
		}
	}
	return NULL;
}

FVector AUTBot::GetEnemyLocation(APawn* TestEnemy, bool bAllowPrediction)
{
	const FBotEnemyInfo* Info = GetEnemyInfo(TestEnemy, true);
	if (Info == NULL)
	{
		return FVector::ZeroVector;
	}
	// return exact loc if seen
	else if (Info->IsCurrentlyVisible(GetWorld()->TimeSeconds))
	{
		return TestEnemy->GetActorLocation();
	}
	else if (!bAllowPrediction || GetWorld()->TimeSeconds - Info->LastFullUpdateTime <= GetWorld()->GetDeltaSeconds())
	{
		return Info->LastKnownLoc;
	}
	else
	{
		// TODO:
		return Info->LastKnownLoc;
	}
}

bool AUTBot::IsEnemyVisible(APawn* TestEnemy)
{
	// only use local enemies for personal visibility
	for (const FBotEnemyInfo& Info : LocalEnemyList)
	{
		if (Info.GetPawn() == TestEnemy)
		{
			return Info.IsCurrentlyVisible(GetWorld()->TimeSeconds);
		}
	}
	return false;
}

bool AUTBot::HasOtherVisibleEnemy()
{
	// only use local enemies for personal visibility
	for (const FBotEnemyInfo& Info : LocalEnemyList)
	{
		if (Info.GetPawn() != Enemy && Info.IsCurrentlyVisible(GetWorld()->TimeSeconds))
		{
			return true;
		}
	}
	return false;
}

bool AUTBot::LostContact(float MaxTime)
{
	if (Enemy == NULL)
	{
		return true;
	}
	else
	{
		// lose invisible enemies faster
		AUTCharacter* EnemyUTChar = Cast<AUTCharacter>(Enemy);
		if (EnemyUTChar != NULL && /*EnemyUTChar->IsInvisible()*/ false)
		{
			MaxTime = FMath::Max<float>(2.0f, MaxTime - 2.0f);
		}

		if (GetWorld()->TimeSeconds - LastEnemyChangeTime <= MaxTime)
		{
			return false;
		}
		else
		{
			// TODO: maybe use team list if enemy is high priority?
			for (const FBotEnemyInfo& Info : LocalEnemyList)
			{
				if (Info.GetPawn() == Enemy)
				{
					return GetWorld()->TimeSeconds - Info.LastFullUpdateTime > MaxTime;
				}
			}

			return true;
		}
	}
}

void AUTBot::SetSquad(AUTSquadAI* NewSquad)
{
	if (NewSquad != NULL && NewSquad->GetTeamNum() != GetTeamNum())
	{
		UE_LOG(UT, Warning, TEXT("AUTBot::SetSquad(): NewSquad is on the wrong team!"));
	}
	else
	{
		if (Squad != NULL)
		{
			Squad->RemoveMember(this);
		}
		Squad = NewSquad;
		if (Squad != NULL)
		{
			Squad->AddMember(this);
		}
	}
}

void AUTBot::WhatToDoNext()
{
	ensure(!bExecutingWhatToDoNext);

	bPendingWhatToDoNext = true;
}

void AUTBot::ExecuteWhatToDoNext()
{
	Target = NULL;

	SwitchToBestWeapon();

	if (GetCharacter() != NULL && GetCharacter()->CharacterMovement->MovementMode == MOVE_Falling)
	{
		StartNewAction(WaitForLandingAction);
	}
	else
	{
		/* TODO: investigate value
		if ((StartleActor != None) && !StartleActor.bDeleteMe)
		{
			StartleActor.GetBoundingCylinder(StartleRadius, StartleHeight);
			if (VSize(StartleActor.Location - Pawn.Location) < StartleRadius)
			{
				Startle(StartleActor);
				return;
			}
		}*/

		// make sure enemy is valid
		if (Enemy != NULL)
		{
			if (Enemy->bPendingKillPending)
			{
				Enemy = NULL;
			}
			AUTCharacter* EnemyUTChar = Cast<AUTCharacter>(Enemy);
			if (EnemyUTChar != NULL && EnemyUTChar->IsDead())
			{
				Enemy = NULL;
			}
		}
		if (Enemy == NULL)
		{
			PickNewEnemy(); // note: not guaranteed to give one
		}
		else
		{
			// maybe lose enemy if haven't had any contact and isn't a high priority
			if (!Squad->MustKeepEnemy(Enemy) && !IsEnemyVisible(Enemy))
			{
				// decide if should lose enemy
				if (/*Squad.IsDefending(self)*/ false)
				{
					if (LostContact(4.0f))
					{
						Squad->LostEnemy(this);
					}
				}
				else if (LostContact(7.0f))
				{
					Squad->LostEnemy(this);
				}
			}
		}

		if ((Squad == NULL || !Squad->CheckSquadObjectives(this)) && !ShouldDefendPosition())
		{
			if (Enemy != NULL && !NeedsWeapon())
			{
				ChooseAttackMode();
			}
			// if no other action, look for items
			if (CurrentAction == NULL)
			{
				if (FindInventoryGoal(0.0f))
				{
					GoalString = FString::Printf(TEXT("Wander to inventory %s"), *GetNameSafe(RouteCache.Last().Actor.Get()));
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
					SetMoveTargetDirect(FRouteCacheItem(GetPawn()->GetActorLocation() + FMath::VRand() * FVector(500.0f, 500.0f, 0.0f)));
					StartNewAction(WaitForMoveAction);
				}
			}
		}
	}
}

void AUTBot::ChooseAttackMode()
{
	float EnemyStrength = RelativeStrength(Enemy);
	AUTWeapon* MyWeap = (GetUTChar() != NULL) ? GetUTChar()->GetWeapon() : NULL;

	/* send under attack voice message if under duress
	if ( EnemyStrength > 0.0f && (PlayerReplicationInfo.Team != None) && (FRand() < 0.25) &&
	(WorldInfo.TimeSeconds - LastInjuredVoiceMessageTime > 45.0) )
	{
		LastInjuredVoiceMessageTime = WorldInfo.TimeSeconds;
		SendMessage(None, 'INJURED', 25);
	}*/
	/*if (Vehicle(Pawn) != None)
	{
		VehicleFightEnemy(true, EnemyStrength);
	}
	else
	*/
	{
		if (/*!bFrustrated && */!Squad->MustKeepEnemy(Enemy))
		{
			float RetreatThreshold = Personality.Aggressiveness;
			if (!NeedsWeapon())
			{
				// low skill bots default to not retreating
				RetreatThreshold += 0.35 - Skill * 0.05f;
			}
			if (EnemyStrength > RetreatThreshold)
			{
				GoalString = "Retreat";
				/* send retreating voice message
				if ((PlayerReplicationInfo.Team != None) && (FRand() < 0.05)
				&& (WorldInfo.TimeSeconds - LastInjuredVoiceMessageTime > 45.0))
				{
				LastInjuredVoiceMessageTime = WorldInfo.TimeSeconds;
				SendMessage(None, 'INJURED', 25);
				}*/
				DoRetreat();
				return;
			}
		}

		if (/*(Squad.PriorityObjective(self) == 0) && */Skill + Personality.Tactics > 2.0f && (EnemyStrength > -0.3f || NeedsWeapon()))
		{
			float WeaponRating;
			if (MyWeap == NULL)
			{
				WeaponRating = 0.0f;
			}
			else if (NeedsWeapon())
			{
				WeaponRating = (EnemyStrength > 0.3f) ? 0.0f : (MyWeap->GetAISelectRating() / 2000.0f);
			}
			else
			{
				WeaponRating = MyWeap->GetAISelectRating() / ((EnemyStrength > 0.3f) ? 2000.0f : 1000.0f);
			}

			// fallback to better pickup?
			if (FindInventoryGoal(WeaponRating))
			{
				GoalString = FString::Printf(TEXT("Fallback to better pickup %s"), *GetNameSafe(RouteCache.Last().Actor.Get()));
				//GotoState('FallBack');
				SetMoveTarget(RouteCache[0]);
				StartNewAction(WaitForMoveAction); // TODO: FallbackAction?
				return;
			}
		}

		GoalString = "ChooseAttackMode FightEnemy";
		FightEnemy(true, EnemyStrength);
	}
}

void AUTBot::FightEnemy(bool bCanCharge, float EnemyStrength)
{
	/*if (Vehicle(Pawn) != None)
	{
		VehicleFightEnemy(bCanCharge, EnemyStrength);
		return;
	}*/
	/*if (Pawn.IsInPain() && FindInventoryGoal(0.0))
	{
		GoalString = "Fallback out of pain volume " $ RouteGoal $ " hidden " $ RouteGoal.bHidden;
		GotoState('FallBack');
		return;
	}*/
	AUTWeapon* MyWeap = (GetUTChar() != NULL) ? GetUTChar()->GetWeapon() : NULL;
	if (MyWeap == NULL)
	{
		if (FindInventoryGoal(0.0f))
		{
			SetMoveTarget(RouteCache[0]);
			//GotoState('FallBack');
			StartNewAction(WaitForMoveAction);
		}
	}
	/*else if ( (Enemy == FailedHuntEnemy) && (WorldInfo.TimeSeconds == FailedHuntTime) )
	{
		GoalString = "FAILED HUNT - HANG OUT";
		if ( LineOfSightTo(Enemy) )
			bCanCharge = false;
		else if ( FindInventoryGoal(0) )
		{
			SetAttractionState();
			return;
		}
		else
		{
			WanderOrCamp();
			return;
		}
	}*/
	else
	{
		bool bOldForcedCharge = false;// bMustCharge;
		//bMustCharge = false;
		const FVector EnemyLoc = GetEnemyLocation(Enemy, true);
		float EnemyDist = (GetPawn()->GetActorLocation() - EnemyLoc).Size();
		bool bFarAway = false;
		// TODO: used to use CombatStyle here... seems almost the same as Aggressiveness, maybe don't need both
		float AdjustedCombatStyle = Personality.Aggressiveness + MyWeap->SuggestAttackStyle();
		CurrentAggression = 1.5f * FMath::FRand() - 0.8f + 2.0f * AdjustedCombatStyle - 0.5 * EnemyStrength
								+ FMath::FRand() * ((Enemy->GetVelocity() - GetPawn()->GetVelocity()).SafeNormal() | (EnemyLoc - GetPawn()->GetActorLocation()).SafeNormal());
		AUTWeapon* EnemyWeap = (Cast<AUTCharacter>(Enemy) != NULL) ? ((AUTCharacter*)Enemy)->GetWeapon() : NULL;
		if (EnemyWeap != NULL)
		{
			CurrentAggression += 2.0f * EnemyWeap->SuggestDefenseStyle();
		}
		//if (enemyDist > MAXSTAKEOUTDIST)
		//	Aggression += 0.5;
		Squad->ModifyAggression(this, CurrentAggression);
		if (GetCharacter() != NULL && GetCharacter()->CharacterMovement != NULL && (GetCharacter()->CharacterMovement->MovementMode == MOVE_Walking || GetCharacter()->CharacterMovement->MovementMode == MOVE_Falling))
		{
			float ZDiff = GetPawn()->GetActorLocation().Z - EnemyLoc.Z;
			if (ZDiff > TacticalHeightAdvantage)
			{
				CurrentAggression = FMath::Max<float>(0.0f, CurrentAggression - 1.0f + AdjustedCombatStyle);
			}
			else if ((Skill < 4.0f || Personality.Aggressiveness >= 0.5f) && EnemyDist > 3000.0f)
			{
				bFarAway = true;
				CurrentAggression += 0.5;
			}
			else if (ZDiff < -GetPawn()->GetSimpleCollisionHalfHeight())  // below enemy
			{
				// unless really aggressive, don't try to charge enemy with substantial height advantage
				if (ZDiff < -TacticalHeightAdvantage && Personality.Aggressiveness < 0.7f)
				{
					CurrentAggression += Personality.Aggressiveness * 0.5f;
				}
				else
				{
					CurrentAggression += Personality.Aggressiveness;
				}
			}
		}

		if (!CanAttack(Enemy, EnemyLoc, true)) // TODO: maybe not bDirectOnly = true? Unsure if best to check for indirect attacks from here
		{
			if (Squad->MustKeepEnemy(Enemy))
			{
				GoalString = "Hunt priority enemy";
				DoHunt();
			}
			else if (!bCanCharge)
			{
				GoalString = "Stake Out - no charge";
				//DoStakeOut();
			}
			/*else if (Squad.IsDefending(self) && LostContact(4) && ClearShot(LastSeenPos, false))
			{
				GoalString = "Stake Out "$LastSeenPos;
				DoStakeOut();
			}*/
			else if (((CurrentAggression < 1.0f && !LostContact(3.0f + 2.0f * FMath::FRand()))/* || IsSniping()*/)/* && CanStakeOut()*/)
			{
				GoalString = "Stake Out2";
				//DoStakeOut();
			}
			else if ( Skill + Personality.Tactics >= 3.5f + FMath::FRand() && !LostContact(1.0f) /*&& VSize(EnemyLoc - GetPawn()->GetActorLocation()) < MAXSTAKEOUTDIST*/ &&
				!NeedsWeapon() && !MyWeap->bMeleeWeapon &&
				FMath::FRand() < 0.75f && !IsEnemyVisible(Enemy) && (Enemy->Controller == NULL || !Enemy->Controller->LineOfSightTo(GetPawn())) &&
				!HasOtherVisibleEnemy() )
			{
				GoalString = "Stake Out 3";
				//DoStakeOut();
			}
			else
			{
				GoalString = "Hunt";
				DoHunt();
			}
		}
		else
		{
			// see enemy - decide whether to charge it or strafe around/stand and fire
			SetFocus(Enemy);

			if (MyWeap->bMeleeWeapon || (bCanCharge && bOldForcedCharge))
			{
				GoalString = "Charge";
				DoCharge();
			}
			/*else if (MyWeap->RecommendLongRangedAttack())
			{
			GoalString = "Long Ranged Attack";
			DoRangedAttackOn(Enemy);
			}*/
			else if (bCanCharge && (Skill < 5.0f || Personality.Aggressiveness >= 0.5f) && bFarAway && CurrentAggression > 1.0f && FMath::FRand() < 0.5)
			{
				GoalString = "Charge closer";
				DoCharge();
			}
			else if (MyWeap->bPrioritizeAccuracy /*|| IsSniping()*/ || (FMath::FRand() > 0.17f * (Skill + Personality.Tactics - 1.0f)/* && !DefendMelee(EnemyDist)*/))
			{
				GoalString = "Ranged Attack";
				DoRangedAttackOn(Enemy);
			}
			else if (bCanCharge && CurrentAggression > 1.0f)
			{
				GoalString = "Charge 2";
				DoCharge();
			}
			else
			{
				GoalString = "Do tactical move";
				if (!MyWeap->bRecommendSplashDamage && FMath::FRand() < 0.7f && 3.0f * Personality.Jumpiness + FMath::FRand() * Skill > 3.0f)
				{
					GoalString = "Try to Duck";
					FVector Y = FRotationMatrix(GetControlRotation()).GetScaledAxis(EAxis::Y);
					if (FMath::FRand() < 0.5f)
					{
						TryEvasiveAction(Y * -1.0f);
					}
					else
					{
						TryEvasiveAction(Y);
					}
				}
				DoTacticalMove();
			}
		}
	}
}

void AUTBot::DoRetreat()
{
	if (Squad->PickRetreatDestination(this))
	{
		//GotoState('Retreating');
		//StartNewAction(RetreatAction);
		StartNewAction(WaitForMoveAction);
	}
	// if nothing, then tactical move
	else if (LineOfSightTo(Enemy))
	{
		//GoalString = "No retreat because frustrated";
		//bFrustrated = true;
		if (GetUTChar() != NULL && GetUTChar()->GetWeapon() != NULL && GetUTChar()->GetWeapon()->bMeleeWeapon)
		{
			DoCharge();
		}
		/*else if (Vehicle(Pawn) != None)
		{
			GotoState('VehicleCharging');
		}*/
		else
		{
			DoTacticalMove();
		}
	}
	else
	{
		//GoalString = "Stakeout because no retreat dest";
		//DoStakeOut();
	}
}

void AUTBot::DoCharge()
{
	if (GetTarget() == NULL)
	{
		UE_LOG(UT, Warning, TEXT("AI ERROR: %s got into DoCharge() with no target!"), *(PlayerState != NULL ? PlayerState->PlayerName : GetName()));
		DoTacticalMove();
	}
	else
	{
		FVector HitLocation;
		if (GetTarget() == Enemy && !NavData->RaycastWithZCheck(GetPawn()->GetNavAgentLocation(), Enemy->GetNavAgentLocation(), GetPawn()->GetSimpleCollisionHalfHeight() * 2.0f, HitLocation))
		{
			SetMoveTargetDirect(FRouteCacheItem(Enemy));
			StartNewAction(ChargeAction);
		}
		else
		{
			float Weight = 0.0f;
			FSingleEndpointEval NodeEval(GetTarget());
			NavData->FindBestPath(GetPawn(), *GetPawn()->GetNavAgentProperties(), NodeEval, GetPawn()->GetNavAgentLocation(), Weight, true, RouteCache);
			if (RouteCache.Num() > 0)
			{
				SetMoveTarget(RouteCache[0]);
				StartNewAction(ChargeAction);
			}
			else
			{
				DoTacticalMove();
			}
		}
	}
}

void AUTBot::DoTacticalMove()
{
	/*if (!Pawn.bCanStrafe)
	{
		if (Pawn.HasRangedAttack())
			DoRangedAttackOn(Enemy);
		else
			WanderOrCamp();
	}
	else
	*/
	{
		StartNewAction(TacticalMoveAction);
	}
}

void AUTBot::DoRangedAttackOn(AActor* NewTarget)
{
	Target = NewTarget;

	// leave vehicle if it's not useful for shooting things
	/*V = UTVehicle(Pawn);
	if (V != None && V.bShouldLeaveForCombat)
	{
		LeaveVehicle(false);
	}*/

	SetFocus(Target);
	StartNewAction(RangedAttackAction);
}

void AUTBot::DoHunt()
{
	if (Enemy == NULL)
	{
		UE_LOG(UT, Warning, TEXT("Bot %s in DoHunt() with no enemy"), *PlayerState->PlayerName);
		DoTacticalMove();
	}
	else
	{
		// TODO: guess future destination, intercept path, try to find high ground, etc
		FSingleEndpointEval NodeEval(GetEnemyLocation(Enemy, true));
		float Weight = 0.0f;
		if (NavData->FindBestPath(GetPawn(), *GetPawn()->GetNavAgentProperties(), NodeEval, GetPawn()->GetNavAgentLocation(), Weight, true, RouteCache))
		{
			SetMoveTarget(RouteCache[0]);
			StartNewAction(ChargeAction); // TODO: hunting action
		}
	}
}

float AUTBot::RelativeStrength(APawn* Other)
{
	const FBotEnemyInfo* Info = GetEnemyInfo(Other, true);
	if (Info == NULL)
	{
		return 0.0f;
	}
	else
	{
		// TODO: account for implicit strength of pawn class (relevant for vehicles - 100% tank health isn't the same as 100% human health)
		float Relation = Info->EffectiveHealthPct - ((UTChar != NULL) ? UTChar->GetEffectiveHealthPct(false) : 1.0f);

		if (UTChar != NULL && UTChar->GetWeapon() != NULL)
		{
			Relation -= 0.5f * UTChar->DamageScaling * UTChar->GetFireRateMultiplier() * UTChar->GetWeapon()->GetAISelectRating();
			if (UTChar->GetWeapon()->BaseAISelectRating < 0.5f)
			{
				Relation += 0.3f;
				if (Info->GetUTChar() != NULL && Info->GetUTChar()->GetWeapon() != NULL && Info->GetUTChar()->GetWeapon()->BaseAISelectRating > 0.5f)
				{
					Relation += 0.3f;
				}
			}
		}
		if (Info->GetUTChar() != NULL && Info->GetUTChar()->GetWeapon() != NULL)
		{
			Relation += 0.5f * Info->GetUTChar()->DamageScaling * Info->GetUTChar()->GetFireRateMultiplier() * Info->GetUTChar()->GetWeapon()->BaseAISelectRating;
		}

		if (GetWorld()->TimeSeconds - Info->LastFullUpdateTime < 10.0f)
		{
			if (Info->LastKnownLoc.Z > GetPawn()->GetActorLocation().Z + TacticalHeightAdvantage)
			{
				Relation += 0.2f;
			}
			else if (GetPawn()->GetActorLocation().Z > Info->LastKnownLoc.Z + TacticalHeightAdvantage)
			{
				Relation -= 0.15;
			}
		}

		return Relation;
	}
}

bool AUTBot::ShouldDefendPosition()
{
	return false; // TODO
}

void AUTBot::UTNotifyKilled(AController* Killer, AController* KilledPlayer, APawn* KilledPawn, const UDamageType* DamageType)
{
	if (KilledPawn == Enemy)
	{
		Enemy = NULL;
		if (GetPawn() != NULL)
		{
			PickNewEnemy();
			if (Killer == this)
			{
				// TODO: maybe taunt
			}
		}
	}
}

void AUTBot::NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, const FDamageEvent& DamageEvent)
{
	if (InstigatedBy != NULL && InstigatedBy != this && InstigatedBy->GetPawn() != NULL && (!AreAIIgnoringPlayers() || Cast<APlayerController>(InstigatedBy) == NULL) && (Enemy == NULL || !LineOfSightTo(Enemy)) && Squad != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(InstigatedBy, this))
		{
			UpdateEnemyInfo(InstigatedBy->GetPawn(), EUT_TookDamage);
		}
	}
}

void AUTBot::NotifyCausedHit(APawn* HitPawn, int32 Damage)
{
	if (HitPawn != GetPawn() && HitPawn->Controller != NULL && !HitPawn->bTearOff)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(HitPawn, this))
		{
			UpdateEnemyInfo(HitPawn, EUT_DealtDamage);
		}
	}
}

void AUTBot::NotifyPickup(APawn* PickedUpBy, AActor* Pickup, float AudibleRadius)
{
	if (GetPawn() != NULL && GetPawn() != PickedUpBy)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(PickedUpBy, this))
		{
			bool bCanUpdateEnemyInfo = Pickup->IsA(AUTPickupHealth::StaticClass());
			if (!bCanUpdateEnemyInfo)
			{
				TSubclassOf<AUTInventory> InventoryType = NULL;

				AUTDroppedPickup* DP = Cast<AUTDroppedPickup>(Pickup);
				if (DP != NULL)
				{
					InventoryType = DP->GetInventoryType();
				}
				else
				{
					AUTPickupInventory* InvPickup = Cast<AUTPickupInventory>(Pickup);
					if (InvPickup != NULL)
					{
						InventoryType = InvPickup->GetInventoryType();
					}
				}
				// assume inventory items that manipulate damage change effective health
				if (InventoryType != NULL)
				{
					bCanUpdateEnemyInfo = InventoryType.GetDefaultObject()->bCallDamageEvents;
				}
			}
			if (bCanUpdateEnemyInfo && (Pickup->GetActorLocation() - GetPawn()->GetActorLocation()).Size() < AudibleRadius * HearingRadiusMult * 0.5f || IsEnemyVisible(PickedUpBy) || LineOfSightTo(Pickup))
			{
				UpdateEnemyInfo(PickedUpBy, EUT_HealthUpdate);
			}
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
			UpdateEnemyInfo(Incoming->Instigator, EUT_TookDamage);
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
			UpdateEnemyInfo(Shooter, EUT_TookDamage);
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
	if (IsStopped())
	{
		StartNewAction(TacticalMoveAction);
	}
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

void AUTBot::PickNewEnemy()
{
	if (Enemy == NULL || Enemy->Controller == NULL || !Squad->MustKeepEnemy(Enemy) || !CanAttack(Enemy, GetEnemyLocation(Enemy, true), false))
	{
		LastPickEnemyTime = GetWorld()->TimeSeconds;

		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		const TArray<const FBotEnemyInfo>& EnemyList = (PS != NULL && PS->Team != NULL) ? PS->Team->GetEnemyList() : *(const TArray<const FBotEnemyInfo>*)&LocalEnemyList;

		LastPickEnemyRatings.Empty();
		APawn* BestEnemy = NULL;
		float BestRating = -10000.0f;
		for (const FBotEnemyInfo& EnemyInfo : EnemyList)
		{
			if (EnemyInfo.IsValid())
			{
				bool bLostEnemy = EnemyInfo.bLostEnemy;
				if (&EnemyList != (const TArray<const FBotEnemyInfo>*)&LocalEnemyList)
				{
					for (const FBotEnemyInfo& LocalEnemyInfo : LocalEnemyList)
					{
						if (LocalEnemyInfo.GetPawn() == EnemyInfo.GetPawn())
						{
							bLostEnemy = LocalEnemyInfo.bLostEnemy;
							break;
						}
					}
				}
				if (!bLostEnemy)
				{
					float Rating = RateEnemy(EnemyInfo);
					new(LastPickEnemyRatings) FBotEnemyRating(EnemyInfo.GetPawn(), Rating);
					if (Rating > BestRating)
					{
						BestEnemy = EnemyInfo.GetPawn();
						BestRating = Rating;
					}
				}
			}
		}
		SetEnemy(BestEnemy);
	}
}

float AUTBot::RateEnemy(const FBotEnemyInfo& EnemyInfo)
{
	float NewStrength = RelativeStrength(EnemyInfo.GetPawn());
	// more likely to pursue strong enemies if aggressive
	float ThreatValue = FMath::Clamp<float>(NewStrength, 0.0f, 1.0f) * Personality.Aggressiveness;
	// more likely to pursue weak enemies if tactical (TODO: some other personality attribute? We check Tactics a lot...)
	if (NewStrength < 0.0f && Personality.Tactics > 0.0f)
	{
		ThreatValue += NewStrength * -0.5f * Personality.Tactics;
	}
	float Dist = (EnemyInfo.LastKnownLoc - GetPawn()->GetActorLocation()).Size();
	if (Dist < 4500.0f)
	{
		ThreatValue += 0.2;
		if (Dist < 3300.0f)
		{
			ThreatValue += 0.2;
			if (Dist < 2200.0f)
			{
				ThreatValue += 0.2;
				if (Dist < 1100.0f)
				{
					ThreatValue += 0.2;
				}
			}
		}
	}

	bool bThreatVisible = IsEnemyVisible(EnemyInfo.GetPawn()); // intentional that we use bot's personal visibility here instead of team
	bool bThreatAttackable = CanAttack(EnemyInfo.GetPawn(), EnemyInfo.LastKnownLoc, false);
	if (bThreatVisible)
	{
		ThreatValue += 1.0f;
		ThreatValue += FMath::Max<float>(0.0f, 1.0f - (GetWorld()->TimeSeconds - EnemyInfo.LastHitByTime / 2.0f));
	}
	else if (bThreatAttackable)
	{
		ThreatValue += 0.5f;
		ThreatValue += FMath::Max<float>(0.0f, 1.0f - (GetWorld()->TimeSeconds - EnemyInfo.LastHitByTime / 2.0f)) * 0.5f;
	}
	if (Enemy != NULL && EnemyInfo.GetPawn() != Enemy)
	{
		if (!bThreatVisible)
		{
			if (!bThreatAttackable)
			{
				ThreatValue -= 5.0f;
			}
			else
			{
				ThreatValue -= 2.0f;
			}
		}
		else if (GetWorld()->TimeSeconds - GetEnemyInfo(Enemy, false)->LastSeenTime > 2.0f)
		{
			ThreatValue += 1;
		}
		if (Dist > 0.7f * (GetEnemyInfo(Enemy, true)->LastKnownLoc - GetPawn()->GetActorLocation()).Size())
		{
			ThreatValue -= 0.25;
		}
		ThreatValue -= 0.2;

		/*if (B.IsHunting() && (NewStrength < 0.2)
			&& (WorldInfo.TimeSeconds - FMax(B.LastSeenTime, B.AcquireTime) < 2.5))
			ThreatValue -= 0.3;*/
	}

	// TODO: further personality adjust (hate for enemy that kills me, enemy that took powerup I was going for, etc)

	return Squad->ModifyEnemyRating(ThreatValue, EnemyInfo, this);
}

void AUTBot::UpdateEnemyInfo(APawn* NewEnemy, EAIEnemyUpdateType UpdateType)
{
	if (NewEnemy != NULL && Squad != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(NewEnemy, this))
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
			if (PS != NULL && PS->Team != NULL)
			{
				PS->Team->UpdateEnemyInfo(NewEnemy, UpdateType);
			}
			bool bFound = false;
			for (int32 i = 0; i < LocalEnemyList.Num(); i++)
			{
				if (!LocalEnemyList[i].IsValid(this))
				{
					LocalEnemyList.RemoveAt(i--, 1);
				}
				else if (LocalEnemyList[i].GetPawn() == NewEnemy)
				{
					LocalEnemyList[i].Update(UpdateType);
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				new(LocalEnemyList) FBotEnemyInfo(NewEnemy, UpdateType);
				PickNewEnemy();
			}
		}
	}
}

void AUTBot::RemoveEnemy(APawn* OldEnemy)
{
	for (TArray<FBotEnemyInfo>::TIterator It(LocalEnemyList); It; ++It)
	{
		if (It->GetPawn() == OldEnemy)
		{
			It->bLostEnemy = true;
			if (OldEnemy == Enemy)
			{
				SetEnemy(NULL);
			}
			break;
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
		if (GetFocusActor() == Enemy)
		{
			SetFocus(NewEnemy);
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
		// make sure we always have local info for enemies we focus on (simplifies decision code)
		if (Enemy != NULL && !GetEnemyInfo(Enemy, false))
		{
			const FBotEnemyInfo* TeamEnemyInfo = GetEnemyInfo(Enemy, true);
			if (TeamEnemyInfo == NULL)
			{
				UpdateEnemyInfo(Enemy, EUT_HeardApprox);
			}
			else
			{
				// copy some details from team
				FBotEnemyInfo* NewEnemyInfo = new(LocalEnemyList) FBotEnemyInfo(*TeamEnemyInfo);
				NewEnemyInfo->LastSeenTime = -100000.0f;
				NewEnemyInfo->LastHitByTime = -100000.0f;
			}
		}
		LastEnemyChangeTime = GetWorld()->TimeSeconds;
		if (bExecutingWhatToDoNext)
		{
			// force update of visibility info if this is during decision logic
			if (Enemy != NULL && CanSee(Enemy, false))
			{
				SeePawn(Enemy);
			}
		}
		else
		{
			WhatToDoNext();
		}
	}
}

void AUTBot::HearSound(APawn* Other, const FVector& SoundLoc, float SoundRadius)
{
	if (Other != GetPawn())
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(Other, this))
		{
			UpdateEnemyInfo(Other, ((SoundLoc - GetPawn()->GetActorLocation()).Size() < SoundRadius * HearingRadiusMult * 0.5f) ? EUT_HeardExact : EUT_HeardApprox);
		}
	}
}

void AUTBot::SetTarget(AActor* NewTarget)
{
	Target = NewTarget;
}

void AUTBot::SeePawn(APawn* Other)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS == NULL || !GS->OnSameTeam(Other, this))
	{
		UpdateEnemyInfo(Other, EUT_Seen);
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
	return (Other == NULL) ? false : UTLineOfSightTo(Other, ViewPoint, bAlternateChecks, Other->GetTargetLocation(GetPawn()));
}
bool AUTBot::UTLineOfSightTo(const AActor* Other, FVector ViewPoint, bool bAlternateChecks, FVector TargetLocation) const
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
		if (TargetLocation.IsZero())
		{
			TargetLocation = Other->GetTargetLocation(GetPawn());
		}

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