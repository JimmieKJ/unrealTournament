// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTProj_ShockBall.h"
#include "UTWeap_ShockRifle.h"
#include "UnrealNetwork.h"
#include "StatNames.h"
#include "UTRewardMessage.h"

AUTProj_ShockBall::AUTProj_ShockBall(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ComboDamageParams = FRadialDamageParams(215.0f, 550.0f);
	ComboAmmoCost = 3;
	bComboExplosion = false;
	bUsingClientSideHits = false;
	ComboMomentum = 330000.0f;
	bIsEnergyProjectile = true;
	PrimaryActorTick.bCanEverTick = true;
	ComboVortexType = AUTPhysicsVortex::StaticClass();
	bMoveFakeToReplicatedPos = false;
}

void AUTProj_ShockBall::InitFakeProjectile(AUTPlayerController* OwningPlayer)
{
	Super::InitFakeProjectile(OwningPlayer);
	TArray<USphereComponent*> Components;
	GetComponents<USphereComponent>(Components);
	for (int32 i = 0; i < Components.Num(); i++)
	{
		if (Components[i] != CollisionComp)
		{
			Components[i]->SetCollisionResponseToAllChannels(ECR_Ignore);
		}
	}
}

void AUTProj_ShockBall::SetForwardTicked(bool bWasForwardTicked)
{
	bUsingClientSideHits = bWasForwardTicked;
}

float AUTProj_ShockBall::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bFakeClientProjectile)
	{
		if (MasterProjectile && !MasterProjectile->IsPendingKillPending())
		{
			MasterProjectile->TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
		}
		return Damage;
	}
	if (ComboTriggerType != NULL && DamageEvent.DamageTypeClass != NULL && DamageEvent.DamageTypeClass->IsChildOf(ComboTriggerType))
	{
		if (Role != ROLE_Authority)
		{
			AUTPlayerController* UTPC = Cast<AUTPlayerController>(EventInstigator);
			if (UTPC)
			{
				UTPC->ServerNotifyProjectileHit(this, GetActorLocation(), DamageCauser, GetWorld()->GetTimeSeconds());
			}
		}
		else if (!bUsingClientSideHits)
		{
			PerformCombo(EventInstigator, DamageCauser);
		}
	}

	return Damage;
}

void AUTProj_ShockBall::NotifyClientSideHit(AUTPlayerController* InstigatedBy, FVector HitLocation, AActor* DamageCauser)
{
	// @TODO FIXMESTEVE - do I limit how far I move combo, so fair to all?
	TArray<USphereComponent*> Components;
	GetComponents<USphereComponent>(Components);
	for (int32 i = 0; i < Components.Num(); i++)
	{
		if (Components[i] != CollisionComp)
		{
			Components[i]->SetCollisionResponseToAllChannels(ECR_Ignore);
		}
	}
	SetActorLocation(HitLocation);
	PerformCombo(InstigatedBy, DamageCauser);
}

void AUTProj_ShockBall::PerformCombo(class AController* InstigatedBy, class AActor* DamageCauser)
{
	//Consume extra ammo for the combo
	if (Role == ROLE_Authority)
	{
		AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
		AUTWeapon* Weapon = Cast<AUTWeapon>(DamageCauser);
		if (Weapon && (!GameMode || GameMode->bAmmoIsLimited || (Weapon->Ammo > 9)))
		{
			Weapon->AddAmmo(-ComboAmmoCost);
		}
	}

	//The player who combos gets the credit
	InstigatorController = InstigatedBy;

	// Replicate combo and execute locally
	bComboExplosion = true;
	OnRep_ComboExplosion();
	Explode(GetActorLocation(), FVector(1.0f, 0.0f, 0.0f));
}

void AUTProj_ShockBall::OnRep_ComboExplosion()
{
	//Swap combo damage and effects
	DamageParams = ComboDamageParams;
	ExplosionEffects = ComboExplosionEffects;
	MyDamageType = ComboDamageType;
	Momentum = ComboMomentum;
}

void AUTProj_ShockBall::OnComboExplode_Implementation()
{
	if (ComboVortexType != NULL)
	{
		GetWorld()->SpawnActor<AUTPhysicsVortex>(ComboVortexType, GetActorLocation(), GetActorRotation());
	}
}

void AUTProj_ShockBall::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp)
{
	if (!bExploded)
	{
		if (MyDamageType == ComboDamageType)
		{
			// special case for combo - get rid of fake projectile
			if (MyFakeProjectile != NULL)
			{
				// to make sure effects play consistently we need to copy over the LastRenderTime
				float FakeLastRenderTime = MyFakeProjectile->GetLastRenderTime();
				TArray<UPrimitiveComponent*> Components;
				GetComponents<UPrimitiveComponent>(Components);
				for (UPrimitiveComponent* Comp : Components)
				{
					Comp->LastRenderTime = FMath::Max<float>(Comp->LastRenderTime, FakeLastRenderTime);
				}

				MyFakeProjectile->Destroy();
				MyFakeProjectile = NULL;
			}
		}
		AUTPlayerController* PC = Cast<AUTPlayerController>(InstigatorController);
		AUTPlayerState* PS = PC ? Cast<AUTPlayerState>(PC->PlayerState) : NULL;
		int32 ComboKillCount = PS ? PS->GetStatsValue(NAME_ShockComboKills) : 0;
		Super::Explode_Implementation(HitLocation, HitNormal, HitComp);
		if (bComboExplosion)
		{
			OnComboExplode();
			if (PC && PS && ComboRewardMessageClass && (PC == InstigatorController))
			{
				RateShockCombo(PC, PS, ComboKillCount);
			}
		}

		// if bot is low skill, delay clearing bot monitoring so that it will occasionally fire for the combo slightly too late - a realistic player mistake
		AUTBot* B = Cast<AUTBot>(InstigatorController);
		if (bPendingKillPending || B == NULL || B->WeaponProficiencyCheck())
		{
			ClearBotCombo();
		}
		else
		{
			FTimerHandle TempHandle;
			GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_ShockBall::ClearBotCombo, 0.2f, false);
		}
	}
}

void AUTProj_ShockBall::RateShockCombo(AUTPlayerController *PC, AUTPlayerState* PS, int32 OldComboKillCount)
{
	int32 KillCount = (PS->GetStatsValue(NAME_ShockComboKills) - OldComboKillCount);
	float ComboScore = 4.f * FMath::Min(KillCount, 3);

	AUTCharacter* Shooter = Cast<AUTCharacter>(PC->GetPawn());
	if (Shooter)
	{
		// difference in angle between shots
		FVector ShootPos = Shooter->GetWeapon()->GetFireStartLoc();
		ComboScore += 100.f * (1.f - (GetVelocity().GetSafeNormal() | (GetActorLocation() - ShootPos).GetSafeNormal()));
		// current movement speed relative to direction, with bonus if falling
		float MovementBonus = (Shooter->GetCharacterMovement()->MovementMode == MOVE_Falling) ? 5.f : 3.f;
		ComboScore += MovementBonus * Shooter->GetVelocity().Size() / 1000.f;
	}
	if ((ComboScore > 8.f) && (KillCount > 0))
	{
		PS->ModifyStatsValue(NAME_AmazingCombos, 1);
		PC->SendPersonalMessage(ComboRewardMessageClass, PS->GetStatsValue(NAME_AmazingCombos));
	}

	ComboScore *= 100.f; // multiply since stats stored as int32
	int32 CurrentComboRating = PS->GetStatsValue(NAME_BestShockCombo);
	if (ComboScore > CurrentComboRating)
	{
		PS->SetStatsValue(NAME_BestShockCombo, ComboScore);
	}
}

void AUTProj_ShockBall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	ClearBotCombo();
}

void AUTProj_ShockBall::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTProj_ShockBall, bComboExplosion, COND_None);
}

void AUTProj_ShockBall::StartBotComboMonitoring()
{
	bMonitorBotCombo = true;
	PrimaryActorTick.SetTickFunctionEnable(true);
}

void AUTProj_ShockBall::ClearBotCombo()
{
	AUTBot* B = Cast<AUTBot>(InstigatorController);
	if (B != NULL)
	{
		if (B->GetTarget() == this)
		{
			B->SetTarget(NULL);
		}
		if (B->GetFocusActor() == this)
		{
			B->SetFocus(B->GetTarget());
		}
	}
	bMonitorBotCombo = false;
}

void AUTProj_ShockBall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bMonitorBotCombo)
	{
		AUTBot* B = Cast<AUTBot>(InstigatorController);
		if (B != NULL)
		{
			AUTWeap_ShockRifle* Rifle = (B->GetUTChar() != NULL) ? Cast<AUTWeap_ShockRifle>(B->GetUTChar()->GetWeapon()) : NULL;
			if (Rifle != NULL && !Rifle->IsFiring())
			{
				switch (B->ShouldTriggerCombo(GetActorLocation(), GetVelocity(), ComboDamageParams))
				{
					case BMS_Abort:
						if (Rifle->ComboTarget == this)
						{
							Rifle->ComboTarget = NULL;
						}
						// if high skill, still monitor just in case
						bMonitorBotCombo = B->WeaponProficiencyCheck() && B->LineOfSightTo(this);
						break;
					case BMS_PrepareActivation:
						if (B->GetTarget() != this)
						{
							B->SetTarget(this);
							B->SetFocus(this);
						}
						break;
					case BMS_Activate:
						if (Rifle->ComboTarget == this)
						{
							Rifle->ComboTarget = NULL;
						}
						if (B->GetFocusActor() == this && !B->NeedToTurn(GetTargetLocation()))
						{
							Rifle->DoCombo();
						}
						else if (B->GetTarget() != this)
						{
							B->SetTarget(this);
							B->SetFocus(this);
						}
						break;
					default:
						break;
				}
			}
		}
	}
}