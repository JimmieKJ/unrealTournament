// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTAIAction_TacticalMove.h"
#include "UTSquadAI.h"

UUTAIAction_TacticalMove::UUTAIAction_TacticalMove(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MinStrafeDist = 450.0f;
}

void UUTAIAction_TacticalMove::Started()
{
	bEnemyWasVisible = GetOuterAUTBot()->IsEnemyVisible(GetEnemy());
	bChangeDir = false;
	bFinalMove = false;
	// TODO: originally this implemented a speed clamp for low skill bots... investigate

	if (GetEnemy() == NULL)
	{
		UE_LOG(UT, Warning, TEXT("Bot %s in TacticalMove with no enemy"), *GetOuterAUTBot()->PlayerState->PlayerName);
	}
	else
	{
		PickDestination();
	}
}

void UUTAIAction_TacticalMove::PickDestination()
{
	bForcedDirection = false;

	if ( GetCharacter() != NULL && GetCharacter()->GetCharacterMovement()->GetPhysicsVolume() != NULL && GetCharacter()->GetCharacterMovement()->GetPhysicsVolume()->bWaterVolume &&
		/*!Pawn.bCanSwim && */ GetPawn()->GetNavAgentPropertiesRef().bCanFly )
	{
		GetOuterAUTBot()->SetMoveTargetDirect(FRouteCacheItem(GetPawn()->GetActorLocation() + 75.0f * (FMath::VRand() + FVector(0.0f, 0.0f, 1.0f)) + FVector(0.0f, 0.0f, 100.0f)));
	}
	else
	{
		FVector EnemyDir = (GetOuterAUTBot()->GetEnemyLocation(GetEnemy(), true) - GetPawn()->GetActorLocation()).GetSafeNormal();
		FVector Y = (EnemyDir ^ FVector(0.0f, 0.0f, 1.0f));

		if (GetCharacter() != NULL && GetCharacter()->GetCharacterMovement()->MovementMode == MOVE_Walking)
		{
			Y.Z = 0;
			EnemyDir.Z = 0;
		}
		else
		{
			EnemyDir.Z = FMath::Max<float>(0.0f, EnemyDir.Z);
		}

		bool bFollowingPlayer = (Cast<APlayerController>(GetSquad()->GetLeader()) != NULL && GetSquad()->GetLeader()->GetPawn() != NULL && (GetPawn()->GetActorLocation() - GetSquad()->GetLeader()->GetPawn()->GetActorLocation()).Size() < 3500.0f);

		float StrafeSize = FMath::Clamp<float>(((2.0f * GetOuterAUTBot()->CurrentAggression + 1.0f) * FMath::FRand() - 0.65f), -0.7f, 0.7f);
		if (GetSquad()->MustKeepEnemy(GetEnemy()))
		{
			StrafeSize = FMath::Max<float>(0.4f * FMath::FRand() - 0.2f, StrafeSize);
		}

		FVector EnemyPart = EnemyDir * StrafeSize;
		StrafeSize = FMath::Max<float>(0.0, 1.0f - FMath::Abs<float>(StrafeSize));
		FVector PickDir = StrafeSize * Y;
		if (bStrafeDir)
		{
			PickDir *= -1.0f;
		}
		if (bFollowingPlayer)
		{
			// try not to get in front of squad leader
			FVector LookDir = GetSquad()->GetLeader()->GetControlRotation().Vector();
			if ( (LookDir | (GetPawn()->GetActorLocation() + (EnemyPart + PickDir) * MinStrafeDist - GetSquad()->GetLeader()->GetPawn()->GetActorLocation())) >
				FMath::Max<float>(0.0f, (LookDir | (GetPawn()->GetActorLocation() + (EnemyPart - PickDir) * MinStrafeDist - GetSquad()->GetLeader()->GetPawn()->GetActorLocation()))) )
			{
				bStrafeDir = !bStrafeDir;
				PickDir *= -1;
			}
		}

		bStrafeDir = !bStrafeDir;

		if (!EngageDirection(EnemyPart + PickDir, false) && !EngageDirection(EnemyPart - PickDir, false))
		{
			bForcedDirection = true;
			ForcedDirTime = GetWorld()->TimeSeconds;
			EngageDirection(EnemyPart + PickDir, true);
		}
	}
}

bool UUTAIAction_TacticalMove::EngageDirection(const FVector& StrafeDir, bool bForced)
{
	// successfully engage direction if can trace out and down
	FVector MinDest = GetPawn()->GetActorLocation() + MinStrafeDist * StrafeDir;
	if (!bForced)
	{
		FVector Extent = GetPawn()->GetSimpleCollisionRadius() * FVector(1.0f, 1.0f, 0.0f);
		Extent.Z = FMath::Max<float>(15.0f, GetPawn()->GetSimpleCollisionHalfHeight() - GetPawn()->GetNavAgentPropertiesRef().AgentStepHeight);

		bool bWantJump = GetCharacter() != NULL && GetCharacter()->CanJump() && (FMath::FRand() < 0.05f * GetOuterAUTBot()->Skill + 0.6f * GetOuterAUTBot()->Personality.Jumpiness || (GetWeapon() != NULL && GetWeapon()->bRecommendSplashDamage && GetOuterAUTBot()->WeaponProficiencyCheck()))
			&& (GetOuterAUTBot()->GetEnemyLocation(GetEnemy(), true).Z - GetEnemy()->GetSimpleCollisionHalfHeight() <= GetPawn()->GetActorLocation().Z + GetPawn()->GetNavAgentPropertiesRef().AgentStepHeight - GetPawn()->GetSimpleCollisionHalfHeight())
			&& !GetOuterAUTBot()->NeedToTurn(GetOuterAUTBot()->GetFocalPoint());
		FCollisionQueryParams TraceParams(FName(TEXT("EngageDirection")), false, GetPawn());
		FCollisionObjectQueryParams HitParams(ECC_WorldStatic);
		HitParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		FHitResult Hit;
		if (GetWorld()->SweepSingle(Hit, GetPawn()->GetActorLocation(), MinDest, FQuat::Identity, FCollisionShape::MakeCapsule(Extent), TraceParams, HitParams) && !bWantJump)
		{
			return false;
		}
		else
		{
			if (GetCharacter() != NULL && GetCharacter()->GetCharacterMovement()->MovementMode == MOVE_Walking)
			{
				Extent.X = FMath::Min<float>(30.0f, 0.5f * GetPawn()->GetSimpleCollisionRadius());
				Extent.Y = Extent.X;
				if (!GetWorld()->SweepSingle(Hit, MinDest, MinDest - (3.0f * GetPawn()->GetNavAgentPropertiesRef().AgentStepHeight) * FVector(0.0f, 0.0f, 1.0f), FQuat::Identity, FCollisionShape::MakeCapsule(Extent), TraceParams, HitParams))
				{
					return false;
				}
			}

			if (bWantJump)
			{
				if (GetWeapon() != NULL && GetWeapon()->bRecommendSplashDamage)
				{
					GetUTChar()->StopFiring();

					// if blew self up, return
					if (GetUTChar() == NULL || GetUTChar()->IsDead())
					{
						return true;
					}
				}

				// try jump move
				//bPlannedJump = true;
				//DodgeLandZ = Pawn.Location.Z;
				//bInDodgeMove = true;
				GetCharacter()->GetCharacterMovement()->Velocity = (MinDest - GetPawn()->GetActorLocation()).GetClampedToMaxSize2D(GetCharacter()->GetCharacterMovement()->GetMaxSpeed());
				GetCharacter()->GetCharacterMovement()->Velocity.Z = 0.0f;
				GetCharacter()->GetCharacterMovement()->DoJump(false);
				if ( (GetOuterAUTBot()->Personality.Jumpiness > 0.0f && FMath::FRand() < GetOuterAUTBot()->Personality.Jumpiness * 0.5f) ||
					 GetOuterAUTBot()->Skill + 2.0f * GetOuterAUTBot()->Personality.Jumpiness > 3.0f + 3.0f * FMath::FRand() )
				{
					GetOuterAUTBot()->bPlannedWallDodge = true;
				}
				GetOuterAUTBot()->SetMoveTargetDirect(FRouteCacheItem(MinDest));
				return true;
			}
		}
	}
	FVector Dest = MinDest + StrafeDir * (0.5 * MinStrafeDist + FMath::Min<float>((GetOuterAUTBot()->GetEnemyLocation(GetEnemy(), true) - GetPawn()->GetActorLocation()).Size(), MinStrafeDist * (FMath::FRand() + FMath::FRand())));
	GetOuterAUTBot()->SetMoveTargetDirect(FRouteCacheItem(Dest));
	return true;
}

void UUTAIAction_TacticalMove::FinalWaitFinished()
{
	if (FMath::FRand() + 0.3f > GetOuterAUTBot()->CurrentAggression)
	{
		GetOuterAUTBot()->SetMoveTargetDirect(FRouteCacheItem(HidingSpot + 4.0 * GetPawn()->GetSimpleCollisionRadius() * (HidingSpot - GetPawn()->GetActorLocation()).GetSafeNormal()));
		bFinalMove = false;
	}
	else
	{
		GetOuterAUTBot()->StartNewAction(NULL);
	}
}

void UUTAIAction_TacticalMove::StartFinalMove()
{
	const FBotEnemyInfo* EnemyInfo = GetOuterAUTBot()->GetEnemyInfo(GetEnemy(), false);
	if (EnemyInfo == NULL)
	{
		// no enemy info, cancel
		GetOuterAUTBot()->StartNewAction(NULL);
	}
	else
	{
		// try to get back to a shooting position
		bFinalMove = true;

		GetOuterAUTBot()->SetMoveTargetDirect(FRouteCacheItem(EnemyInfo->LastSeeingLoc + 4.0f * GetPawn()->GetSimpleCollisionRadius() * (EnemyInfo->LastSeeingLoc - GetPawn()->GetActorLocation()).GetSafeNormal()));
	}
}

bool UUTAIAction_TacticalMove::Update(float DeltaTime)
{
	if (GetEnemy() == NULL)
	{
		return true;
	}
	else if (GetOuterAUTBot()->GetMoveTarget().IsValid())
	{
		return false;
	}
	else if (bFinalMove)
	{
		if (GetWorld()->GetTimerManager().IsTimerActive(FinalWaitFinishedHandle))
		{
			// timer will handle clearing the action if we want to
			return false;
		}
		// if we can fire now that we got out of hiding spot, do so and maybe loop action
		else if (GetOuterAUTBot()->CanAttack(GetEnemy(), GetOuterAUTBot()->GetEnemyLocation(GetEnemy(), true), false))
		{
			GetOuterAUTBot()->CheckWeaponFiring(false);
			GetWorld()->GetTimerManager().SetTimer(FinalWaitFinishedHandle, this, &UUTAIAction_TacticalMove::FinalWaitFinished, 0.1f + 0.3f * FMath::FRand() + 0.06f * (7.0f - FMath::Min<float>(7.0f, GetOuterAUTBot()->Skill)), false);
			return false;
		}
		else
		{
			return true;
		}
	}
	else if (GetWorld()->GetTimerManager().IsTimerActive(StartFinalMoveHandle))
	{
		// timer will handle next step
		return false;
	}
	else if (bForcedDirection && GetWorld()->TimeSeconds - ForcedDirTime < 0.2f)
	{
		/*if (!Pawn.HasRangedAttack() || Skill > 2 + 3 * FRand())
		{
			bMustCharge = true;
			return true;
		}
		else*/
		{
			GetOuterAUTBot()->GoalString = "RangedAttack from failed tactical";
			GetOuterAUTBot()->DoRangedAttackOn(GetEnemy());
			return true;
		}
	}
	else if ( !bEnemyWasVisible || GetOuterAUTBot()->CurrentAggression < GetOuterAUTBot()->RelativeStrength(GetEnemy()) || GetOuterAUTBot()->CanAttack(GetEnemy(), GetOuterAUTBot()->GetEnemyLocation(GetEnemy(), true), false) ||
				(GetUTChar() != NULL && GetUTChar()->GetWeapon() != NULL && GetUTChar()->GetWeapon()->bMeleeWeapon) )
	{
		return true;
	}
	else
	{
		GetOuterAUTBot()->GoalString = "Recover Enemy";
		HidingSpot = GetPawn()->GetActorLocation();
		if (GetUTChar() != NULL && GetUTChar()->GetWeapon() != NULL && (!GetUTChar()->GetWeapon()->IsFiring() || !GetUTChar()->GetWeapon()->IsChargedFireMode(GetUTChar()->GetWeapon()->GetCurrentFireMode())))
		{
			GetUTChar()->StopFiring();
		}
		GetWorld()->GetTimerManager().SetTimer(StartFinalMoveHandle, this, &UUTAIAction_TacticalMove::StartFinalMove, 0.1f + 0.2f * FMath::FRand(), false);
		return false;
	}
}

bool UUTAIAction_TacticalMove::NotifyMoveBlocked(const FHitResult& Impact)
{
	/*if (Vehicle(Wall) != None && Vehicle(Pawn) == None)
	{
		if (Wall == RouteGoal || (Vehicle(RouteGoal) != None && Wall == Vehicle(RouteGoal).GetVehicleBase()))
		{
			V = UTVehicle(Wall);
			if (V != None)
			{
				V.TryToDrive(Pawn);
				if (Vehicle(Pawn) != None)
				{
					Squad.BotEnteredVehicle(self);
					WhatToDoNext();
				}
			}
			return true;
		}
		return false;
	}
	else */if (GetCharacter() != NULL && GetCharacter()->GetCharacterMovement()->MovementMode == MOVE_Falling)
	{
		return false;
	}
	else if (bChangeDir || FMath::FRand() < 0.5 || ((GetOuterAUTBot()->GetEnemyLocation(GetEnemy(), true) - GetPawn()->GetActorLocation()) | Impact.Normal) < 0.0f)
	{
		GetOuterAUTBot()->WhatToDoNext();
		return true;
	}
	else
	{
		bChangeDir = true;
		GetOuterAUTBot()->SetMoveTargetDirect(FRouteCacheItem(GetPawn()->GetActorLocation() - Impact.Normal * FMath::FRand() * 500.0f));
		return true;
	}
}