// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTAIAction_RangedAttack.h"
#include "UTSquadAI.h"

UUTAIAction_RangedAttack::UUTAIAction_RangedAttack(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

void UUTAIAction_RangedAttack::Started()
{
	GetOuterAUTBot()->ClearMoveTarget();
	if (GetUTChar() != NULL && (GetUTChar()->GetWeapon() == NULL || GetUTChar()->GetWeapon()->bMeleeWeapon))
	{
		GetOuterAUTBot()->SwitchToBestWeapon();
	}
	if (GetEnemy() != NULL)
	{
		//CheckIfShouldCrouch(GetPawn()->GetActorLocation(), Cast<Pawn>(GetTarget()) != NULL ? GetOuterAUTBot()->GetEnemyLocation((APawn*)GetTarget(), GetTarget() == GetEnemy()) : GetTarget()->GetActorLocation());
	}
	GetWorld()->GetTimerManager().SetTimer(this, &UUTAIAction_RangedAttack::FirstShotTimer, 0.2f, false);
}

bool UUTAIAction_RangedAttack::FindStrafeDest()
{
	// TODO
	return false;
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
		if (GetEnemy() != NULL)
		{
			//CheckIfShouldCrouch(GetPawn()->GetActorLocation(), Cast<Pawn>(GetTarget()) != NULL ? GetOuterAUTBot()->GetEnemyLocation((APawn*)GetTarget(), GetTarget() == GetEnemy()) : GetTarget()->GetActorLocation());
		}
		if (!FindStrafeDest())
		{
			GetWorld()->GetTimerManager().SetTimer(this, &UUTAIAction_RangedAttack::EndTimer, 0.2f + (0.5f + 0.5f * FMath::FRand()) * 0.4f * (7.0f - GetOuterAUTBot()->Skill), false);
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
									!GetWorld()->GetTimerManager().IsTimerActive(this, &UUTAIAction_RangedAttack::FirstShotTimer) &&
									!GetWorld()->GetTimerManager().IsTimerActive(this, &UUTAIAction_RangedAttack::EndTimer) &&
									(GetUTChar() == NULL || GetUTChar()->GetWeapon() == NULL || !GetUTChar()->GetWeapon()->IsPreparingAttack()) );
}