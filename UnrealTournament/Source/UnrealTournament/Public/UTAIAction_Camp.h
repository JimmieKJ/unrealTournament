// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAIAction.h"
#include "UTBot.h"
#include "UTDefensePoint.h"

#include "UTAIAction_Camp.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTAIAction_Camp : public UUTAIAction
{
	GENERATED_BODY()

	UUTAIAction_Camp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	float CampEndTime;

	virtual void Started() override
	{
		GetOuterAUTBot()->HearingRadiusMult *= 1.25f;
		GetOuterAUTBot()->PeripheralVision -= 0.1f;

		CampEndTime = GetWorld()->TimeSeconds + 3.0f;
		// if camping pickup, set end time to re-evaluate right after it respawns
		TArray<AActor*> Touching;
		GetPawn()->GetOverlappingActors(Touching, AUTPickup::StaticClass());
		for (AActor* Touched : Touching)
		{
			AUTPickup* Pickup = Cast<AUTPickup>(Touched);
			if (Pickup != NULL)
			{
				float RespawnOffset = Pickup->GetRespawnTimeOffset(GetPawn());
				if (RespawnOffset > 0.0f)
				{
					CampEndTime = FMath::Min<float>(CampEndTime, GetWorld()->TimeSeconds + RespawnOffset);
				}
			}
		}
	}

	virtual void Ended(bool bAborted) override
	{
		Super::Ended(bAborted);

		GetOuterAUTBot()->ResetPerceptionProperties();
	}

	virtual bool SetFocusForNoTarget()
	{
		if (GetOuterAUTBot()->GetDefensePoint() != NULL && GetUTNavData(GetWorld())->HasReachedTarget(GetOuterAUTBot()->GetPawn(), GetOuterAUTBot()->GetPawn()->GetNavAgentPropertiesRef(), FRouteCacheItem(GetOuterAUTBot()->GetDefensePoint())))
		{
			GetOuterAUTBot()->SetFocalPoint(GetOuterAUTBot()->GetDefensePoint()->GetActorLocation() + GetOuterAUTBot()->GetDefensePoint()->GetActorRotation().Vector() * 1000.0f);
		}
		else
		{
			const TArray<const FBotEnemyInfo>& EnemyList = GetOuterAUTBot()->GetEnemyList(true);
			if (EnemyList.Num() == 0)
			{
				GetOuterAUTBot()->SetFocalPoint(GetOuterAUTBot()->GetPawn()->GetActorLocation() + GetOuterAUTBot()->GetPawn()->GetActorRotation().Vector() * 1000.0f);
			}
			else
			{
				FVector AvgEnemyLoc = FVector::ZeroVector;
				for (const FBotEnemyInfo& TestEnemy : EnemyList)
				{
					AvgEnemyLoc += TestEnemy.LastKnownLoc / EnemyList.Num();
				}
				GetOuterAUTBot()->SetFocalPoint(AvgEnemyLoc);
			}
		}
		return true;
	}

	virtual bool Update(float DeltaTime) override
	{
		return GetWorld()->TimeSeconds >= CampEndTime;
	}
};