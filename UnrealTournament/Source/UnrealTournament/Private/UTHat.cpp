// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTDmg_SniperHeadshot.h"
#include "UTHat.h"


AUTHat::AUTHat(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{	
	HeadshotRotationRate.Yaw = 900;
	HeadshotRotationTime = 0.8f;
	bDontDropOnDeath=false;
}

void AUTHat::OnWearerHeadshot_Implementation()
{
	bHeadshotRotating = true;
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTHat::HeadshotRotationComplete, HeadshotRotationTime, false);
}

void AUTHat::OnWearerDeath_Implementation(TSubclassOf<UDamageType> DamageType)
{
	DetachRootComponentFromParent(true);
	
	if (DamageType->IsChildOf(UUTDmg_SniperHeadshot::StaticClass()))
	{
		OnWearerHeadshot();
	}
	else
	{
		SetBodiesToSimulatePhysics();
	}
}

void AUTHat::HeadshotRotationComplete()
{
	bHeadshotRotating = false;
	SetBodiesToSimulatePhysics();
}

void AUTHat::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bHeadshotRotating)
	{
		GetRootComponent()->AddLocalRotation(DeltaSeconds * HeadshotRotationRate);
	}
}