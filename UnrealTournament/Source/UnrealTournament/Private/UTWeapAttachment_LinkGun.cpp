// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeapAttachment_LinkGun.h"

void AUTWeapAttachment_LinkGun::FiringExtraUpdated()
{
	if (UTOwner->FlashExtra > 0 && UTOwner->FireMode == 1 && WeaponType != NULL)
	{
		// display beam pulse
		LastBeamPulseTime = GetWorld()->TimeSeconds;
		// use an extra muzzle flash slot at the end for the pulse effect
		int32 PulseFlashIndex = WeaponType.GetDefaultObject()->FiringState.Num();
		if (MuzzleFlash.IsValidIndex(PulseFlashIndex) && MuzzleFlash[PulseFlashIndex] != NULL)
		{
			PulseTarget = nullptr;
			TArray<FOverlapResult> Hits;
			GetWorld()->OverlapMultiByChannel(Hits, UTOwner->FlashLocation, FQuat::Identity, COLLISION_TRACE_WEAPON, FCollisionShape::MakeSphere(10.0f), FCollisionQueryParams(NAME_None, true, UTOwner));
			for (const FOverlapResult& Hit : Hits)
			{
				if (Cast<APawn>(Hit.Actor.Get()) != nullptr)
				{
					PulseTarget = Hit.Actor.Get();
				}
			}
			if (PulseTarget != nullptr)
			{
				MuzzleFlash[PulseFlashIndex]->SetTemplate(PulseSuccessEffect);
				MuzzleFlash[PulseFlashIndex]->SetActorParameter(FName(TEXT("Player")), PulseTarget);
			}
			else
			{
				MuzzleFlash[PulseFlashIndex]->SetTemplate(PulseFailEffect);
			}
		}
	}
	else
	{
		PulseTarget = nullptr;
	}
}

void AUTWeapAttachment_LinkGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MuzzleFlash.IsValidIndex(1) && MuzzleFlash[1] != NULL)
	{
		static FName NAME_PulseScale(TEXT("PulseScale"));
		float NewScale = 1.0f + FMath::Max<float>(0.0f, 1.0f - (GetWorld()->TimeSeconds - LastBeamPulseTime) / 0.35f);
		MuzzleFlash[1]->SetVectorParameter(NAME_PulseScale, FVector(NewScale, NewScale, NewScale));
	}
}

