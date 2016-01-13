// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTReplicatedEmitter.h"
#include "UTPersistentReplicatedEmitter.h"
#include "Particles/ParticleSystemComponent.h"

AUTReplicatedEmitter::AUTReplicatedEmitter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PSC = ObjectInitializer.CreateDefaultSubobject<UParticleSystemComponent>(this, TEXT("Particles"));
	PSC->SecondsBeforeInactive = 0.0f;
	PSC->OnSystemFinished.AddDynamic(this, &AUTReplicatedEmitter::OnParticlesFinished);
	RootComponent = PSC;
	InitialLifeSpan = 10.0f;
	DedicatedServerLifeSpan = 0.5f;

	SetReplicates(true);
	bReplicateMovement = true;
	bNetTemporary = true;
}

AUTPersistentReplicatedEmitter::AUTPersistentReplicatedEmitter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bNetTemporary = false;
	InitialLifeSpan = 0.f;
	DedicatedServerLifeSpan = 0.f;
}
