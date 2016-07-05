// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTAIAction_RangedAttack.h"
#include "UTSquadAI.h"

UUTAIAction_RangedAttack::UUTAIAction_RangedAttack(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UUTAIAction_RangedAttack::Started()
{
	bEnemyWasAttackable = IsEnemyAttackable();

	GetOuterAUTBot()->ClearMoveTarget();
	if (GetUTChar() != NULL && (GetUTChar()->GetWeapon() == NULL || GetUTChar()->GetWeapon()->bMeleeWeapon))
	{
		GetOuterAUTBot()->SwitchToBestWeapon();
	}
	// maybe crouch to throw off opponent aim
	if (GetEnemy() != NULL && GetCharacter() != NULL && GetWeapon() != NULL && GetWeapon()->bPrioritizeAccuracy && (GetOuterAUTBot()->GetEnemyLocation(GetEnemy(), false) - GetPawn()->GetActorLocation()).Size() > 4000.0f &&
		FMath::FRand() < FMath::Max<float>(5.0f, GetOuterAUTBot()->Skill) * 0.05f - 0.1f - (GetOuterAUTBot()->Personality.Jumpiness - 0.5f))
	{
		GetCharacter()->GetCharacterMovement()->bWantsToCrouch = true;
	}
	SetTimerUFunc(this, TEXT("FirstShotTimer"), 0.2f, false);
}

void UUTAIAction_RangedAttack::Ended(bool bAborted)
{
	Super::Ended(bAborted);
	if (GetCharacter() != NULL) // could have ended because we're dead
	{
		GetCharacter()->GetCharacterMovement()->bWantsToCrouch = false;
	}
}

bool UUTAIAction_RangedAttack::FindStrafeDest()
{
	if (/*!GetCharacter()->bCanStrafe || */ GetWeapon() == NULL || GetOuterAUTBot()->Skill + GetOuterAUTBot()->Personality.MovementAbility < 1.5f + 3.5f * FMath::FRand())
	{
		// can't strafe, no weapon to check distance with or not skilled enough
		return false;
	}
	else
	{
		AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
		NavNodeRef MyPoly = (NavData != NULL) ? NavData->UTFindNearestPoly(GetCharacter()->GetNavAgentLocation(), GetCharacter()->GetSimpleCollisionCylinderExtent()) : INVALID_NAVNODEREF;
		if (MyPoly != INVALID_NAVNODEREF)
		{
			TArray<NavNodeRef> AdjacentPolys;
			NavData->FindAdjacentPolys(GetPawn(), GetPawn()->GetNavAgentPropertiesRef(), MyPoly, true, AdjacentPolys);
			if (AdjacentPolys.Num() > 0)
			{
				const FVector TargetLoc = GetTarget()->GetActorLocation();
				float TargetDist = (TargetLoc - GetPawn()->GetActorLocation()).Size();
				// consider backstep opposed to always charging if enemy objective, depending on combat style and weapon
				bool bAllowBackwards = true;
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS != NULL && !GS->OnSameTeam(GetTarget(), GetOuterAUTBot()))
				{
					bAllowBackwards = (GetOuterAUTBot()->Personality.Aggressiveness + GetWeapon()->SuggestAttackStyle() <= 0.0);
				}
				float MaxRange = /* TODO: (UTWeap != None ? UTWeap.GetOptimalRangeFor(Focus) : Pawn.Weapon.MaxRange()) */ 10000.0f;
				FCollisionQueryParams TraceParams(FName(TEXT("FindStrafeDest")), false, GetPawn());
				TraceParams.AddIgnoredActor(GetTarget());
				// pick a random point linked to anchor that we can shoot target from
				int32 Start = FMath::RandHelper(AdjacentPolys.Num());
				int32 i = Start;
				do
				{
					const FVector TestLoc = NavData->GetPolyCenter(AdjacentPolys[i]);
					// allow points within range, that aren't significantly backtracking unless allowed,
					// and that we can still hit target from
					float Dist = (TestLoc - TargetLoc).Size();
					if ( (Dist <= MaxRange || Dist < TargetDist) && (bAllowBackwards || Dist <= TargetDist + 250.0) &&
						!GetWorld()->LineTraceTestByChannel(TestLoc + FVector(0.0f, 0.0f, GetPawn()->GetSimpleCollisionHalfHeight()), TargetLoc, ECC_Visibility, TraceParams) )
					{
						GetOuterAUTBot()->SetMoveTargetDirect(FRouteCacheItem(TestLoc, AdjacentPolys[i]));
						return true;
					}
					i++;
					if (i == AdjacentPolys.Num())
					{
						i = 0;
					}
				} while (i != Start);
			}
		}
		return false;
	}
}

void UUTAIAction_RangedAttack::FirstShotTimer()
{
	if ( (GetUTChar() != NULL && GetUTChar()->GetWeapon() != NULL && GetUTChar()->GetWeapon()->bMeleeWeapon) || GetTarget() == NULL || GetTarget() != GetOuterAUTBot()->GetFocusActor() ||
		(GetTarget() != GetEnemy() && GetTarget() != GetSquad()->GetObjective() && GetEnemy() != NULL && GetOuterAUTBot()->LineOfSightTo(GetEnemy())) )
	{
		GetOuterAUTBot()->WhatToDoNext();
	}
	else
	{
		// maybe crouch to throw off opponent aim
		if (GetEnemy() != NULL && GetCharacter() != NULL && GetWeapon() != NULL && GetWeapon()->bPrioritizeAccuracy && FMath::FRand() < 0.5f - GetOuterAUTBot()->Skill * 0.05f  - 0.5f * GetOuterAUTBot()->Personality.Jumpiness)
		{
			GetCharacter()->GetCharacterMovement()->bWantsToCrouch = true;
		}
		if (!FindStrafeDest())
		{
			SetTimerUFunc(this, TEXT("EndTimer"), 0.2f + (0.5f + 0.5f * FMath::FRand()) * 0.4f * (7.0f - GetOuterAUTBot()->Skill), false);
		}
	}
}

void UUTAIAction_RangedAttack::EndTimer()
{
	// just used as a marker
}

bool UUTAIAction_RangedAttack::Update(float DeltaTime)
{
	return GetTarget() == NULL || ( !GetOuterAUTBot()->GetMoveTarget().IsValid() &&
									!IsTimerActiveUFunc(this, TEXT("FirstShotTimer")) &&
									!IsTimerActiveUFunc(this, TEXT("EndTimer")) &&
									(GetUTChar() == NULL || GetUTChar()->GetWeapon() == NULL || !GetUTChar()->GetWeapon()->IsPreparingAttack()) );
}