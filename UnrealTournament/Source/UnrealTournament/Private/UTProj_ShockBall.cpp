// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTProj_ShockBall.h"
#include "UnrealNetwork.h"

AUTProj_ShockBall::AUTProj_ShockBall(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	ComboDamageParams = FRadialDamageParams(215.0f, 550.0f);
	ComboAmmoCost = 3;
	bComboExplosion = false;
	ComboMomentum = 330000.0f;
	bIsEnergyProjectile = true;
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

void AUTProj_ShockBall::ReceiveAnyDamage(float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, class AActor* DamageCauser)
{
	if (bFakeClientProjectile)
	{
		if (MasterProjectile && !MasterProjectile->IsPendingKillPending())
		{
			MasterProjectile->ReceiveAnyDamage(Damage, DamageType, InstigatedBy, DamageCauser);
		}
		return;
	}
	if (ComboTriggerType != NULL && DamageType != NULL && DamageType->IsA(ComboTriggerType))
	{
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(InstigatedBy);
		// @TODO FIXMESTEVE- we really need client to replicate his ping to server so there is never disagreement about whether using client-side hits - then can get rid of (PredictionTime < 15.f) below
		float PredictionTime = UTPC != nullptr ? UTPC->GetPredictionTime() : 0.0f;
		bool bUsingClientSideHits = UTPC && (PredictionTime > 0.f);

		if ((Role == ROLE_Authority) && (!bUsingClientSideHits || (PredictionTime < 15.f)))
		{
			PerformCombo(InstigatedBy, DamageCauser);
		}
		else if ((Role != ROLE_Authority) && bUsingClientSideHits)
		{
			UTPC->ServerNotifyProjectileHit(this, GetActorLocation(), DamageCauser, GetWorld()->GetTimeSeconds());
		}
	}
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
	AUTWeapon* Weapon = Cast<AUTWeapon>(DamageCauser);
	if (Weapon != NULL)
	{
		Weapon->AddAmmo(-ComboAmmoCost);
	}

	//The player who combos gets the credit
	InstigatorController = InstigatedBy;

	// Replicate combo and execute locally
	bComboExplosion = true;
	OnRep_ComboExplosion();
	Explode(GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
}

void AUTProj_ShockBall::OnRep_ComboExplosion()
{
	//Swap combo damage and effects
	DamageParams = ComboDamageParams;
	ExplosionEffects = ComboExplosionEffects;
	MyDamageType = ComboDamageType;
	Momentum = ComboMomentum;
}

void AUTProj_ShockBall::OnComboExplode_Implementation(){}

void AUTProj_ShockBall::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp)
{
	if (!bExploded)
	{
		if (MyDamageType == ComboDamageType)
		{
			// special case for combo - get rid of fake projectile
			if (MyFakeProjectile)
			{
				MyFakeProjectile->Destroy();
				MyFakeProjectile = NULL;
			}
		}
		Super::Explode_Implementation(HitLocation, HitNormal, HitComp);
		if (bComboExplosion)
		{
			OnComboExplode();
		}
	}
}

void AUTProj_ShockBall::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTProj_ShockBall, bComboExplosion, COND_None);
}