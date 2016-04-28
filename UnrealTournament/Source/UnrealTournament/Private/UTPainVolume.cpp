// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTPainVolume.h"

AUTPainVolume::AUTPainVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bWaterVolume = true;
	FluidFriction = 0.5f;
	PawnEntryVelZScaling = 0.8f;
	BrakingDecelerationSwimming = 2000.f;
	TerminalVelocity = 3000.f;
	bEntryPain = false;
}

void AUTPainVolume::AddOverlayMaterials_Implementation(AUTGameState* GS) const
{
	if (InVolumeEffect.IsValid())
	{
		GS->AddOverlayEffect(InVolumeEffect);
	}
}

void AUTPainVolume::ActorEnteredVolume(class AActor* Other)
{
	if (Other)
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		if (P)
		{
			if (InVolumeEffect.IsValid())
			{
				P->SetCharacterOverlayEffect(InVolumeEffect, true);
			}

			if (bWaterVolume || EntrySound)
			{
				P->PlayWaterSound(EntrySound ? EntrySound : P->CharacterData.GetDefaultObject()->WaterEntrySound);
			}
			if (bWaterVolume && P->GetCharacterMovement())
			{
				P->GetCharacterMovement()->Velocity.Z *= PawnEntryVelZScaling;
				P->GetCharacterMovement()->BrakingDecelerationSwimming = BrakingDecelerationSwimming;
			}
		}
		else if (EntrySound)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), EntrySound, Other, SRT_None);
		}
		AUTCarriedObject* Flag = Cast<AUTCarriedObject>(Other);
		if (bPainCausing && Flag)
		{
			Flag->EnteredPainVolume(this);
		}
		if (bPainCausing && bEntryPain && Other->bCanBeDamaged)
		{
			CausePainTo(Other);
		}
	}
}

void AUTPainVolume::ActorLeavingVolume(class AActor* Other)
{
	if (Other)
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		if (P)
		{
			if (InVolumeEffect.IsValid())
			{
				P->SetCharacterOverlayEffect(InVolumeEffect, false);
			}

			if (bWaterVolume || ExitSound)
			{
				P->PlayWaterSound(ExitSound ? ExitSound : P->CharacterData.GetDefaultObject()->WaterExitSound);
			}
		}
		Super::ActorLeavingVolume(Other);
	}
}

void AUTPainVolume::PainTimer()
{
	if (bPainCausing)
	{
		TArray<AActor*> TouchingActors;
		GetOverlappingActors(TouchingActors, APawn::StaticClass());

		for (int32 iTouching = 0; iTouching < TouchingActors.Num(); ++iTouching)
		{
			AActor* const A = TouchingActors[iTouching];
			if (A && A->bCanBeDamaged && !A->IsPendingKill())
			{
				CausePainTo(A);
			}
		}
	}
}

void AUTPainVolume::BeginPlay()
{
	GetWorldTimerManager().SetTimer(PainTimerHandle, this, &AUTPainVolume::PainTimer, PainInterval, true);
	Super::BeginPlay();
}
