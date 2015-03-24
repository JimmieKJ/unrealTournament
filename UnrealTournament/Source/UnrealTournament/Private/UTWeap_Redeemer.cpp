// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Redeemer.h"
#include "UTSquadAI.h"
#include "UTWeaponState.h"
#include "UTWeaponStateEquipping.h"
#include "UTWeaponStateUnequipping.h"

AUTWeap_Redeemer::AUTWeap_Redeemer(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ClassicGroup = 10;
	BringUpTime = 1.1f;
	PutDownTime = 0.72f;
	Ammo = 1;
	MaxAmmo = 1;
	FiringViewKickback = -50.f;
	bMustBeHolstered = true;
	BasePickupDesireability = 1.5f;
	BaseAISelectRating = 1.5f;
	FOVOffset = FVector(1.f, 3.f, 3.f);
}

AUTProjectile* AUTWeap_Redeemer::FireProjectile()
{
	if (GetUTOwner() == NULL)
	{
		UE_LOG(UT, Warning, TEXT("%s::FireProjectile(): Weapon is not owned (owner died during firing sequence)"));
		return NULL;
	}
	else if (Role == ROLE_Authority)
	{
		if (CurrentFireMode == 0)
		{
			return Super::FireProjectile();
		}
		else
		{
			// try and fire a projectile
			const FVector SpawnLocation = GetFireStartLoc();
			const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);

			//DrawDebugSphere(GetWorld(), SpawnLocation, 10, 10, FColor::Green, true);

			UTOwner->IncrementFlashCount(CurrentFireMode);

			// spawn the projectile at the muzzle
			FActorSpawnParameters Params;
			Params.Instigator = UTOwner;
			AUTRemoteRedeemer* RemoteRedeemer = GetWorld()->SpawnActor<AUTRemoteRedeemer>(RemoteRedeemerClass, SpawnLocation, SpawnRotation, Params);
			if (RemoteRedeemer)
			{
				if (UTOwner && UTOwner->Controller)
				{
					RemoteRedeemer->SetOwner(UTOwner->Controller);
					RemoteRedeemer->ForceReplication();
					RemoteRedeemer->TryToDrive(UTOwner);
				}

				RemoteRedeemer->CollisionComp->bGenerateOverlapEvents = true;
			}
			else
			{
				UE_LOG(UT, Warning, TEXT("Could not spawn remote redeemer"));
			}

			return NULL;
		}
	}
	else
	{
		return NULL;
	}
}

float AUTWeap_Redeemer::SuggestAttackStyle_Implementation()
{
	return -1.0f;
}
float AUTWeap_Redeemer::SuggestDefenseStyle_Implementation()
{
	return -1.0f;
}

void AUTWeap_Redeemer::BringUp(float OverflowTime)
{
	Super::BringUp(OverflowTime);
	UUTGameplayStatics::UTPlaySound(GetWorld(), BringupSound, UTOwner, SRT_AllButOwner);
}

bool AUTWeap_Redeemer::PutDown()
{
	if (Super::PutDown())
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), PutDownSound, UTOwner, SRT_AllButOwner);
		return true;
	}
	return false;
}

float AUTWeap_Redeemer::GetAISelectRating_Implementation()
{
	AUTBot* B = Cast<AUTBot>(GetUTOwner()->Controller);
	if (B == NULL)
	{
		return BaseAISelectRating;
	}
	// TODO:
	//else if (B->IsShootingObjective())
	else if (false)
	{
		return 2.0f;
	}
	else
	{
		float ExplosionRadius = 4000.0f;
		if (ProjClass.Num() > 0 && ProjClass[0] != NULL)
		{
			ExplosionRadius = ProjClass[0].GetDefaultObject()->DamageParams.OuterRadius;
		}
		// avoid switching to at suicide range unless it will kill more enemies/high priority enemy and bot thinks it can get away with the long switch time
		if (B->GetEnemy() == NULL || (UTOwner->GetWeapon() != this && !B->GetSquad()->MustKeepEnemy(B->GetEnemy()) && (B->GetEnemyLocation(B->GetEnemy(), false) - UTOwner->GetActorLocation()).Size() < ExplosionRadius * 1.1f))
		{
			if (B->IsEnemyVisible(B->GetEnemy()) && B->Skill + B->Personality.Tactics < 3.0f + 1.5f * FMath::FRand() && (B->Personality.Aggressiveness <= 0.0f || FMath::FRand() > B->Personality.Aggressiveness))
			{
				return 0.4f;
			}
			else
			{
				TArray<APawn*> Enemies = B->GetEnemiesNear(B->GetEnemyLocation(B->GetEnemy(), true), ExplosionRadius * 0.75f, true);
				if (Enemies.Num() > 2)
				{
					return BaseAISelectRating;
				}
				else
				{
					for (APawn* TestEnemy : Enemies)
					{
						if (B->GetSquad()->MustKeepEnemy(TestEnemy))
						{
							return BaseAISelectRating;
						}
					}
					return 0.4f;
				}
			}
		}
		else
		{
			return BaseAISelectRating;
		}
	}
}

bool AUTWeap_Redeemer::CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc)
{
	bool bResult = Super::CanAttack_Implementation(Target, TargetLoc, bDirectOnly, bPreferCurrentMode, BestFireMode, OptimalTargetLoc);
	// TODO: support guided fire
	BestFireMode = 0;
	return bResult;
}
