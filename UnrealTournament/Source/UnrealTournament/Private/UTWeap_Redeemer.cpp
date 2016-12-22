// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Redeemer.h"
#include "UTSquadAI.h"
#include "UTWeaponState.h"
#include "UTWeaponStateEquipping.h"
#include "UTWeaponStateUnequipping.h"
#include "StatNames.h"
#include "UTRedeemerLaunchAnnounce.h"
#include "UTFlagRunGameState.h"
#include "UTGameMessage.h"

AUTWeap_Redeemer::AUTWeap_Redeemer(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DefaultGroup = 10;
	BringUpTime = 0.9f;
	PutDownTime = 0.6f;
	Ammo = 1;
	MaxAmmo = 1;
	FiringViewKickback = -50.f;
	FiringViewKickbackY = 60.f;
	bMustBeHolstered = true;
	BasePickupDesireability = 1.5f;
	BaseAISelectRating = 1.5f;
	FOVOffset = FVector(1.f, 3.f, 3.f);
	bRecommendSplashDamage = true;

	KillStatsName = NAME_RedeemerKills;
	DeathStatsName = NAME_RedeemerDeaths;
	HitsStatsName = NAME_RedeemerHits;
	ShotsStatsName = NAME_RedeemerShots;

	AmmoWarningAmount = 0;
	AmmoDangerAmount = 0;
	FireSoundAmp = SAT_None;

	PickupSpawnAnnouncement = UUTRedeemerLaunchAnnounce::StaticClass();
	PickupAnnouncementIndex = 2;
	bShouldAnnounceDrops = true;

	WeaponCustomizationTag = EpicWeaponCustomizationTags::Redeemer;
	WeaponSkinCustomizationTag = EpicWeaponSkinCustomizationTags::Redeemer;

	TutorialAnnouncements.Add(TEXT("PriRedeemer"));
	TutorialAnnouncements.Add(TEXT("SecRedeemer"));
	HighlightText = NSLOCTEXT("Weapon", "RedeemerHighlightText", "Fear no Evil");
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
		UTOwner->bCanRally = true;
		AUTPlayerState* PS = UTOwner->Controller ? Cast<AUTPlayerState>(UTOwner->Controller->PlayerState) : NULL;
		LaunchTeam = PS && PS->Team ? PS->Team->TeamIndex : 255;
		if (CurrentFireMode == 0)
		{
			LaunchedMissile = Super::FireProjectile();
			if (LaunchedMissile != nullptr)
			{
				FTimerHandle TempHandle;
				GetWorldTimerManager().SetTimer(TempHandle, this, &AUTWeap_Redeemer::AnnounceLaunch, 0.1f, false);
			}
			return Cast<AUTProjectile>(LaunchedMissile);
		}
		else
		{
			const FVector SpawnLocation = GetFireStartLoc();
			const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
			UTOwner->IncrementFlashCount(CurrentFireMode);
			if (PS && (ShotsStatsName != NAME_None))
			{
				PS->ModifyStatsValue(ShotsStatsName, 1);
			}

			// spawn the projectile at the muzzle
			FActorSpawnParameters Params;
			Params.Instigator = UTOwner;
			RemoteRedeemer = GetWorld()->SpawnActor<AUTRemoteRedeemer>(RemoteRedeemerClass, SpawnLocation, SpawnRotation, Params);
			if (!RemoteRedeemer)
			{
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				RemoteRedeemer = GetWorld()->SpawnActor<AUTRemoteRedeemer>(RemoteRedeemerClass, UTOwner->GetActorLocation(), SpawnRotation, Params);
			}
			if (RemoteRedeemer)
			{
				if (UTOwner && UTOwner->Controller)
				{
					RemoteRedeemer->SetOwner(UTOwner->Controller);
					RemoteRedeemer->ForceReplication();
					RemoteRedeemer->TryToDrive(UTOwner);
				}

				RemoteRedeemer->CollisionComp->bGenerateOverlapEvents = true;
				LaunchedMissile = RemoteRedeemer;
				if (LaunchedMissile != nullptr)
				{
					FTimerHandle TempHandle;
					GetWorldTimerManager().SetTimer(TempHandle, this, &AUTWeap_Redeemer::AnnounceLaunch, 0.5f, false);
				}
			}
			else
			{
				UE_LOG(UT, Warning, TEXT("Could not spawn remote redeemer"));
			}
		}
	}
	return NULL;
}

void AUTWeap_Redeemer::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);
	if (NewOwner)
	{
		NewOwner->bCanRally = false;
	}
}

void AUTWeap_Redeemer::DropFrom(const FVector& StartLocation, const FVector& TossVelocity)
{
	if (UTOwner)
	{
		UTOwner->bCanRally = true;
	}
	Super::DropFrom(StartLocation, TossVelocity);
}

void AUTWeap_Redeemer::AnnounceLaunch()
{
	if (LaunchedMissile && !LaunchedMissile->IsPendingKillPending() && !LaunchedMissile->bTearOff && (Cast<AUTProjectile>(LaunchedMissile) ? !Cast<AUTProjectile>(LaunchedMissile)->bExploded : (RemoteRedeemer && !RemoteRedeemer->bExploded)))
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
			if (PC)
			{
				int32 MessageIndex = (PC->GetTeamNum() == LaunchTeam) ? 1 : 0;
				PC->ClientReceiveLocalizedMessage(UUTRedeemerLaunchAnnounce::StaticClass(), MessageIndex);
			}
		}
	}
}

void AUTWeap_Redeemer::AddAmmo(int32 Amount)
{
	if (RemoteRedeemer && !RemoteRedeemer->IsPendingKillPending())
	{
		return;
	}
	Super::AddAmmo(Amount);
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
	UUTGameplayStatics::UTPlaySound(GetWorld(), GlobalBringupSound, UTOwner, SRT_AllButOwner);
}

bool AUTWeap_Redeemer::PutDown()
{
/*	if ((Ammo > 0) && GetWorld()->GetGameState<AUTFlagRunGameState>())
	{
		if (UTOwner && Cast<AUTPlayerController>(UTOwner->GetController()))
		{
			Cast<AUTPlayerController>(UTOwner->GetController())->ClientReceiveLocalizedMessage(UUTGameMessage::StaticClass(), 99);
		}
		return false;
	}*/
	if (Super::PutDown())
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), GlobalPutDownSound, UTOwner, SRT_AllButOwner);
		return true;
	}
	return false;
}

void AUTWeap_Redeemer::DetachFromOwner_Implementation()
{
	Super::DetachFromOwner_Implementation();

	//If we have used up all our redeemer ammo and are putting it down, go ahead and destroy the redeemer 
	// as you should not be able to give it more ammo
	if (Ammo <= 0)
	{
		// wait till put down complete
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTWeap_Redeemer::RemoveRedeemer, 0.02f, false);
	}
}

void AUTWeap_Redeemer::RemoveRedeemer()
{
	//If we have used up all our redeemer ammo and are putting it down, go ahead and destroy the redeemer 
	// as you should not be able to give it more ammo
	if (UTOwner)
	{
		UTOwner->RemoveInventory(this);
	}
	Destroy();
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
		if (B->GetEnemy() == NULL || (UTOwner->GetWeapon() != this && !B->GetSquad()->MustKeepEnemy(B, B->GetEnemy()) && (B->GetEnemyLocation(B->GetEnemy(), false) - UTOwner->GetActorLocation()).Size() < ExplosionRadius * 1.1f))
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
						if (B->GetSquad()->MustKeepEnemy(B, TestEnemy))
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

