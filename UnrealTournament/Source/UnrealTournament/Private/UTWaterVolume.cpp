// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWaterVolume.h"
#include "UTPainVolume.h"
#include "UTCarriedObject.h"

AUTWaterVolume::AUTWaterVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bWaterVolume = true;
	FluidFriction = 0.3f;
	PawnEntryVelZScaling = 0.4f;
	BrakingDecelerationSwimming = 300.f;
	TerminalVelocity = 3000.f;
	WaterCurrentDirection = FVector(0.f);
	MaxRelativeSwimSpeed = 1000.f;
	WaterCurrentSpeed = 0.f;
}

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

void AUTWaterVolume::ActorEnteredVolume(class AActor* Other)
{
	if (Other)
	{
		AUTCharacter* P = Cast<AUTCharacter>(Other);
		if (P)
		{
			P->EnteredWater(this);
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
		Super::ActorLeavingVolume(Other);
	}
}

FVector AUTWaterVolume::GetCurrentFor_Implementation(AActor* Actor) const
{
	return WaterCurrentDirection.GetSafeNormal() * WaterCurrentSpeed;
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
			P->PlayWaterSound(ExitSound ? ExitSound : P->WaterExitSound);
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



