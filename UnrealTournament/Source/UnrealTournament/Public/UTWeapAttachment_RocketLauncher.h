// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponAttachment.h"
#include "Particles/ParticleSystemComponent.h"
#include "Animation/AnimInstance.h"
#include "UTWeapAttachment_RocketLauncher.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API AUTWeapAttachment_RocketLauncher : public AUTWeaponAttachment
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UParticleSystemComponent* RocketLoadLights;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<UAnimMontage*> RocketLoadMontages;

	AUTWeapAttachment_RocketLauncher(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		RocketLoadLights = ObjectInitializer.CreateOptionalDefaultSubobject<UParticleSystemComponent>(this, FName(TEXT("RocketLoadLights")));
		RocketLoadLights->SetupAttachment(Mesh);
	}

	virtual void FiringExtraUpdated() override
	{
		//The lower 4 bits is the number of rockets charged 
		int32 NumRockets = UTOwner->FlashExtra & 0x0F;

		if (RocketLoadLights != NULL)
		{
			// we don't know the previous state so deactivate and retrigger every time
			RocketLoadLights->DeactivateSystem();
			RocketLoadLights->KillParticlesForced();

			if (NumRockets > 0)
			{
				uint8 NumLoadedRockets = (UTOwner->FlashExtra & 0x0F) - 1;
				static FName NAME_RocketMesh(TEXT("RocketMesh"));
				RocketLoadLights->SetActorParameter(NAME_RocketMesh, this);
				RocketLoadLights->ActivateSystem();
				for (int32 i = 1; i <= NumLoadedRockets; i++)
				{
					FName ParticleEvent = FName(*FString::Printf(TEXT("Barrel%i"), i));
					RocketLoadLights->GenerateParticleEvent(ParticleEvent, 0.0f, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector);
				}
			}
		}
		if (Mesh != NULL && Mesh->GetAnimInstance() != NULL)
		{
			if (UTOwner->FlashExtra > 0 && RocketLoadMontages.IsValidIndex(NumRockets - 1))
			{
				Mesh->GetAnimInstance()->Montage_Play(RocketLoadMontages[NumRockets - 1]);
			}
			else
			{
				Mesh->GetAnimInstance()->Montage_Stop(0.0f);
			}
		}
	}

	/** The number of rockets fired is the Replicated by FireMode. So 3 rockets is FireMode=3 */
	virtual void PlayFiringEffects()
	{
		RocketLoadLights->DeactivateSystem();
		RocketLoadLights->KillParticlesForced();
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
