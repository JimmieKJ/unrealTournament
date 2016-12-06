// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProj_Rocket.h"
#include "UTInventory.h"

#include "UTRocketSalvoBoost.generated.h"

UCLASS()
class AUTRocketSalvoBoost : public AUTTimedPowerup
{
	GENERATED_BODY()
public:
	AUTRocketSalvoBoost(const FObjectInitializer& OI)
		: Super(OI)
	{
		TimeRemaining = 1.4f;
		TriggeredTime = 1.4f;
		TargetingRange = 6000.0f;
	}

	/** the projectile to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AUTProj_Rocket> ProjClass;
	/** range to look for enemies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TargetingRange;

	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) override
	{
		FTimerHandle Unused;
		GetWorldTimerManager().SetTimer(Unused, this, &AUTRocketSalvoBoost::FireSalvo, 0.4f, true);
		Super::GivenTo(NewOwner, bAutoActivate);
	}
	virtual void Removed()
	{
		Super::Removed();
		GetWorldTimerManager().ClearAllTimersForObject(this);
	}

	UFUNCTION()
	virtual void FireSalvo()
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		FActorSpawnParameters Params;
		Params.Instigator = UTOwner;
		const FVector SpawnLoc = UTOwner->GetActorLocation() + FVector(0.0f, 0.0f, UTOwner->GetSimpleCollisionHalfHeight() * 1.2f);
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			if (It->IsValid() && 
				It->Get() != UTOwner && 
				!It->Get()->bTearOff && 
				(It->Get()->GetActorLocation() - UTOwner->GetActorLocation()).Size() < TargetingRange && 
				(GS == nullptr || !GS->OnSameTeam(It->Get(), UTOwner)))
			{
				AUTProj_Rocket* Rocket = GetWorld()->SpawnActor<AUTProj_Rocket>(ProjClass, SpawnLoc, FRotator(90.0f, 0.0f, 0.0f), Params);
				if (Rocket != nullptr)
				{
					Rocket->TargetActor = It->Get();
				}
			}
		}
	}
};