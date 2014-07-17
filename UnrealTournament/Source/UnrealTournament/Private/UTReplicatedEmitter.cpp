// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTReplicatedEmitter.h"
#include "Particles/ParticleSystemComponent.h"

AUTReplicatedEmitter(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	PSC = PCIP.CreateDefaultSubobject<UParticleSystemComponent>(this, TEXT("Particles"));
	PSC->OnSystemFinished.BindDynamic(this, &AUTReplicatedEmitter::OnParticlesFinished);
	RootComponent = PSC;
	InitialLifeSpan = 10.0f;
	DedicatedServerLifeSpan = 0.5f;

	SetReplicates(true);
	bReplicateMovement = true;
	bNetTemporary = true;
}