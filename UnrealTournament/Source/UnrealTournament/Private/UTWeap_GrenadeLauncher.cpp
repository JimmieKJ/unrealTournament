// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UnrealNetwork.h"
#include "StatNames.h"
#include "UTWeap_GrenadeLauncher.h"

AUTWeap_GrenadeLauncher::AUTWeap_GrenadeLauncher()
{
	DefaultGroup = 3;
	Group = 3;
	WeaponCustomizationTag = EpicWeaponCustomizationTags::GrenadeLauncher;

	DetonationAfterFireDelay = 0.3f;

	ShotsStatsName = NAME_BioLauncherShots;
	HitsStatsName = NAME_BioLauncherHits;
	HighlightText = NSLOCTEXT("Weapon", "GrenadeHighlightText", "Hot Potato");
}

bool AUTWeap_GrenadeLauncher::BeginFiringSequence(uint8 FireModeNum, bool bClientFired)
{
	// Don't fire until it's time
	if (GetWorld()->TimeSeconds - LastGrenadeFireTime < DetonationAfterFireDelay)
	{
		return false;
	}
	else if (bHasStickyGrenades && FireModeNum == 1)
	{
		if (Role == ROLE_Authority)
		{
			DetonateStickyGrenades();
		}
		
		if (UTOwner && UTOwner->IsLocallyViewed())
		{
			PlayDetonationEffects();
		}

		return true;
	}
	else
	{
		return Super::BeginFiringSequence(FireModeNum, bClientFired);
	}
}

void AUTWeap_GrenadeLauncher::RegisterStickyGrenade(AUTProj_Grenade_Sticky* InGrenade)
{
	ActiveGrenades.AddUnique(InGrenade);
	bHasStickyGrenades = true;
	OnRep_HasStickyGrenades();
	ActiveStickyGrenadeCount = ActiveGrenades.Num();
}

void AUTWeap_GrenadeLauncher::UnregisterStickyGrenade(AUTProj_Grenade_Sticky* InGrenade)
{
	ActiveGrenades.Remove(InGrenade);
	if (ActiveGrenades.Num() == 0)
	{
		bHasStickyGrenades = false;
		OnRep_HasStickyGrenades();
	}
	ActiveStickyGrenadeCount = ActiveGrenades.Num();
}

void AUTWeap_GrenadeLauncher::DetonateStickyGrenades()
{
	for (int i = 0; i < ActiveGrenades.Num(); i++)
	{
		if (ActiveGrenades[i] != nullptr)
		{
			ActiveGrenades[i]->ArmGrenade();
			ActiveGrenades[i]->Explode(ActiveGrenades[i]->GetActorLocation(), FVector(0, 0, 1), nullptr);
		}
	}
	ActiveGrenades.Empty();
	bHasStickyGrenades = false;
	OnRep_HasStickyGrenades();
	ActiveStickyGrenadeCount = ActiveGrenades.Num();
}

void AUTWeap_GrenadeLauncher::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTWeap_GrenadeLauncher, bHasStickyGrenades);
	DOREPLIFETIME(AUTWeap_GrenadeLauncher, ActiveStickyGrenadeCount);
}

void AUTWeap_GrenadeLauncher::BringUp(float OverflowTime)
{
	Super::BringUp(OverflowTime);

	if (bHasStickyGrenades)
	{
		UE_LOG(UT, Verbose, TEXT("%s AUTWeap_GrenadeLauncher::BringUp ShowDetonatorUI()"), *GetName());
		ShowDetonatorUI();
	}
}

bool AUTWeap_GrenadeLauncher::PutDown()
{
	if (Super::PutDown())
	{
		UE_LOG(UT, Verbose, TEXT("%s AUTWeap_GrenadeLauncher::PutDown HideDetonatorUI()"), *GetName());
		HideDetonatorUI();
		return true;
	}

	return false;
}

void AUTWeap_GrenadeLauncher::OnRep_HasStickyGrenades()
{
	if (bHasStickyGrenades)
	{
		LastGrenadeFireTime = GetWorld()->TimeSeconds;

		if (CurrentState != InactiveState && GetOwner() != nullptr)
		{
			UE_LOG(UT, Verbose, TEXT("%s AUTWeap_GrenadeLauncher::OnRep_HasStickyGrenades ShowDetonatorUI()"), *GetName());
			ShowDetonatorUI();
		}
	}
	else
	{
		UE_LOG(UT, Verbose, TEXT("%s AUTWeap_GrenadeLauncher::OnRep_HasStickyGrenades HideDetonatorUI()"), *GetName());
		HideDetonatorUI();
	}

	if (!bHasStickyGrenades)
	{
		SwitchToBestWeaponIfNoAmmo();
	}
}

void AUTWeap_GrenadeLauncher::Destroyed()
{
	Super::Destroyed();

	UE_LOG(UT, Verbose, TEXT("%s AUTWeap_GrenadeLauncher::Destroyed HideDetonatorUI()"), *GetName());
	HideDetonatorUI();
}

void AUTWeap_GrenadeLauncher::DetachFromOwner_Implementation()
{
	UE_LOG(UT, Verbose, TEXT("%s AUTWeap_GrenadeLauncher::DetachFromOwner_Implementation HideDetonatorUI()"), *GetName());
	HideDetonatorUI();

	Super::DetachFromOwner_Implementation();
}

void AUTWeap_GrenadeLauncher::DropFrom(const FVector& StartLocation, const FVector& TossVelocity)
{
	DetonateStickyGrenades();

	Super::DropFrom(StartLocation, TossVelocity);
}

void AUTWeap_GrenadeLauncher::ConsumeAmmo(uint8 FireModeNum)
{
	if (FireModeNum == 1)
	{
		bFiringStickyGrenade = true;
	}

	Super::ConsumeAmmo(FireModeNum);

	bFiringStickyGrenade = false;
}

void AUTWeap_GrenadeLauncher::OnRep_Ammo()
{
	// Skip auto weapon switch if there's sticky grenades out
	if (bHasStickyGrenades || bFiringStickyGrenade)
	{
		return;
	}

	Super::OnRep_Ammo();
}

bool AUTWeap_GrenadeLauncher::CanSwitchTo()
{
	if (bHasStickyGrenades)
	{
		return true;
	}

	return Super::CanSwitchTo();
}