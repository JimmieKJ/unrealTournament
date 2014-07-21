// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponAttachment.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTWeapAttachment_RocketLauncher.generated.h"

/**
 * 
 */
UCLASS(CustomConstructor)
class AUTWeapAttachment_RocketLauncher : public AUTWeaponAttachment
{
	GENERATED_UCLASS_BODY()

	AUTWeapAttachment_RocketLauncher(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}
	/**The number of rockets fired is the Replicated by FireMode. So 3 rockets is FireMode=3*/
	virtual void PlayFiringEffects()
	{
		if (UTOwner->FireMode > 0)
		{
			// stop any firing effects for other firemodes
			// this is needed because if the user swaps firemodes very quickly replication might not receive a discrete stop and start new
			StopFiringEffects(true);

			for (int32 i = 0; i < UTOwner->FireMode; i++)
			{
				// muzzle flash
				if (MuzzleFlash.IsValidIndex(i) && MuzzleFlash[i] != NULL && MuzzleFlash[i]->Template != NULL)
				{
					// if we detect a looping particle system, then don't reactivate it
					if (!MuzzleFlash[i]->bIsActive || MuzzleFlash[i]->Template->Emitters[0] == NULL || IsLoopingParticleSystem(MuzzleFlash[i]->Template))
					{
						MuzzleFlash[i]->ActivateSystem();
					}
				}
			}
		}
		else
		{
			Super::PlayFiringEffects();
		}
	}
	
};
