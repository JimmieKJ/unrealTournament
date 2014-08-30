// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponAttachment.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTWeapAttachment_Sniper.generated.h"

UCLASS(CustomConstructor)
class AUTWeapAttachment_Sniper : public AUTWeaponAttachment
{
	GENERATED_UCLASS_BODY()

	AUTWeapAttachment_Sniper(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}
	
	virtual void PlayFiringEffects()
	{
		Super::PlayFiringEffects();

		// FIXME: temp hack for effects parity
		if (!UTOwner->FlashLocation.IsZero() && MuzzleFlash.IsValidIndex(UTOwner->FireMode) && MuzzleFlash[UTOwner->FireMode] != NULL)
		{
			UParticleSystemComponent* PSC = NewObject<UParticleSystemComponent>(this);
			PSC->bAutoActivate = true;
			PSC->bAutoDestroy = true;
			PSC->SetWorldLocationAndRotation(MuzzleFlash[UTOwner->FireMode]->GetComponentLocation(), MuzzleFlash[UTOwner->FireMode]->GetComponentRotation());
			PSC->SetTemplate(LoadObject<UParticleSystem>(NULL, TEXT("ParticleSystem'/Game/RestrictedAssets/Weapons/Sniper/Assets/Smoke_Trail_Bullet.Smoke_Trail_Bullet'")));
			PSC->RegisterComponent();
			PSC->TickComponent(0.0f, ELevelTick::LEVELTICK_All, NULL);
			PSC->SetWorldLocation(UTOwner->FlashLocation);
			PSC->TickComponent(0.0f, ELevelTick::LEVELTICK_All, NULL);
			PSC->DeactivateSystem();
		}
	}
};
