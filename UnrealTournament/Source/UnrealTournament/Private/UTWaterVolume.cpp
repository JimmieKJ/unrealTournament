// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWaterVolume.h"
#include "UTPainVolume.h"

AUTWaterVolume::AUTWaterVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bWaterVolume = true;
	FluidFriction = 0.3f;
	PawnEntryVelZScaling = 0.4f;
	BrakingDecelerationSwimming = 300.f;
	TerminalVelocity = 3000.f;

}

AUTPainVolume::AUTPainVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bWaterVolume = true;
	FluidFriction = 0.5f;
	PawnEntryVelZScaling = 0.4f;
	BrakingDecelerationSwimming = 2000.f;
	TerminalVelocity = 3000.f;
	bEntryPain = false;
}

void AUTWaterVolume::ActorEnteredVolume(class AActor* Other)
{
	if (Other)
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		if (P)
		{
			P->PlayWaterSound(EntrySound ? EntrySound : P->WaterEntrySound);
			if (P->GetCharacterMovement())
			{
				P->GetCharacterMovement()->Velocity.Z *= PawnEntryVelZScaling;
				P->GetCharacterMovement()->BrakingDecelerationSwimming = BrakingDecelerationSwimming;
			}
		}
		else if (EntrySound)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), EntrySound, Other, SRT_None);
		}
		Super::ActorEnteredVolume(Other);
	}
}

void AUTWaterVolume::ActorLeavingVolume(class AActor* Other)
{
	if (Other)
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		if (P)
		{
			P->PlayWaterSound(ExitSound ? ExitSound : P->WaterExitSound);
		}
		else if (ExitSound)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), ExitSound, Other, SRT_None);
		}
		Super::ActorLeavingVolume(Other);
	}
}

void AUTPainVolume::ActorEnteredVolume(class AActor* Other)
{
	if (Other)
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		if (P)
		{
			P->PlayWaterSound(EntrySound ? EntrySound : P->WaterEntrySound);
			if (P->GetCharacterMovement())
			{
				P->GetCharacterMovement()->Velocity.Z *= PawnEntryVelZScaling;
				P->GetCharacterMovement()->BrakingDecelerationSwimming = BrakingDecelerationSwimming;
			}
		}
		else if (EntrySound)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), EntrySound, Other, SRT_None);
		}
		if (bPainCausing && bEntryPain && Other->bCanBeDamaged)
		{
			CausePainTo(Other);
		}

		// Start timer if none is active
		if (!GetWorldTimerManager().IsTimerActive(this, &AUTPainVolume::PainTimer))
		{
			GetWorldTimerManager().SetTimer(this, &AUTPainVolume::PainTimer, PainInterval, true);
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
			P->PlayWaterSound(ExitSound ? ExitSound : P->WaterExitSound);
		}
		else if (ExitSound)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), ExitSound, Other, SRT_None);
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

		// Stop timer if nothing is overlapping us
		if (TouchingActors.Num() == 0)
		{
			GetWorldTimerManager().ClearTimer(this, &AUTPainVolume::PainTimer);
		}
	}
}



