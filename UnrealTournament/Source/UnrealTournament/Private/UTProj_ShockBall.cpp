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
}

void AUTProj_ShockBall::ReceiveAnyDamage(float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, class AActor* DamageCauser)
{
	if (Role == ROLE_Authority && ComboTriggerType != NULL && DamageType != NULL && DamageType->IsA(ComboTriggerType))
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

		Explode(GetActorLocation(), FVector(0.0f,0.0f,1.0f));
	}
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