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
	ImmuneTeamIndex = -1;
	VolumeName = NSLOCTEXT("Volume", "PainVolume", "Pain");
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


bool AUTPainVolume::EncompassesPoint(FVector Point, float SphereRadius/*=0.f*/, float* OutDistanceToPoint)
{
	return Super::EncompassesPoint(Point, SphereRadius, OutDistanceToPoint);
}

void AUTPainVolume::PostUnregisterAllComponents()
{
	// Route clear to super first.
	Super::PostUnregisterAllComponents();
	// World will be NULL during exit purge.
	if (GetWorld())
	{
		GetWorld()->PostProcessVolumes.RemoveSingle(this);
	}
}

// @note: This is a duplicate function from UnrealClient.cpp - should be refactored
void UTInsertVolume(IInterface_PostProcessVolume* Volume, TArray< IInterface_PostProcessVolume* >& VolumeArray)
{
	const int32 NumVolumes = VolumeArray.Num();
	float TargetPriority = Volume->GetProperties().Priority;
	int32 InsertIndex = 0;
	// TODO: replace with binary search.
	for (; InsertIndex < NumVolumes; InsertIndex++)
	{
		IInterface_PostProcessVolume* CurrentVolume = VolumeArray[InsertIndex];
		float CurrentPriority = CurrentVolume->GetProperties().Priority;

		if (TargetPriority < CurrentPriority)
		{
			break;
		}
		if (CurrentVolume == Volume)
		{
			return;
		}
	}
	VolumeArray.Insert(Volume, InsertIndex);
}

void AUTPainVolume::PostRegisterAllComponents()
{
	// Route update to super first.
	Super::PostRegisterAllComponents();
	UTInsertVolume(this, GetWorld()->PostProcessVolumes);
}

void AUTPainVolume::CausePainTo(AActor* Other)
{
	bool bIsImmune = false;
	if (ImmuneTeamIndex >= 0)
	{
		AUTCharacter* UTChar = Cast<AUTCharacter>(Other);
		bIsImmune = UTChar && (UTChar->GetTeamNum() == ImmuneTeamIndex);
	}

	if (!bIsImmune)
	{
		Super::CausePainTo(Other);
	}
}