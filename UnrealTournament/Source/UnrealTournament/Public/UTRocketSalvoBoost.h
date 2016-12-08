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
		TimeRemaining = 1.0f;
		TriggeredTime = 1.0f;
		TargetingRange = 8000.0f;
		MaxTargets = 10;
	}

	/** the projectile to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AUTProj_Rocket> ProjClass;
	/** range to look for enemies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TargetingRange;
	/** maximum number of targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxTargets;

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
		const FVector MyLoc = UTOwner->GetActorLocation();
		TArray<APawn*> Targets;
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			if (It->IsValid() && 
				It->Get() != UTOwner && 
				!It->Get()->bTearOff && 
				(It->Get()->GetActorLocation() - MyLoc).Size() < TargetingRange &&
				(GS == nullptr || !GS->OnSameTeam(It->Get(), UTOwner)))
			{
				Targets.Add(It->Get());
			}
			Targets.Sort([this, MyLoc](const APawn& A, const APawn& B) { return (A.GetActorLocation() - MyLoc).Size() < (B.GetActorLocation() - MyLoc).Size(); });
			FActorSpawnParameters Params;
			Params.Instigator = UTOwner;
			const FVector SpawnLoc = UTOwner->GetActorLocation() + UTOwner->GetActorRotation().Vector() * UTOwner->GetSimpleCollisionRadius() + FVector(0.0f, 0.0f, UTOwner->GetSimpleCollisionHalfHeight() * 0.9f);
			for (int32 i = FMath::Min<int32>(Targets.Num(), MaxTargets) - 1; i >= 0; i--)
			{
				AUTProj_Rocket* Rocket = GetWorld()->SpawnActor<AUTProj_Rocket>(ProjClass, SpawnLoc, FRotator(90.0f, 0.0f, 0.0f), Params);
				if (Rocket != nullptr)
				{
					Rocket->TargetActor = Targets[i];
				}
			}
		}
	}
};