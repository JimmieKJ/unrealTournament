// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTFlagReturnTrail.h"
#include "Particles/ParticleSystemComponent.h"

AUTFlagReturnTrail::AUTFlagReturnTrail(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	InitialLifeSpan = 1.5f;
	DedicatedServerLifeSpan = 0.25f; 
	TeamIndex = 255;

	PSC->bAutoActivate = true;
	PSC->bAutoDestroy = false;

	static ConstructorHelpers::FObjectFinder<UParticleSystem> TrailEffect(TEXT("ParticleSystem'/Game/RestrictedAssets/Effects/CTF/Particles/PS_Flag_Trail.PS_Flag_Trail'"));
	PSC->SetTemplate(TrailEffect.Object);
}

void AUTFlagReturnTrail::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTFlagReturnTrail, EndPoint);
	DOREPLIFETIME(AUTFlagReturnTrail, TeamIndex);
}

void AUTFlagReturnTrail::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector Dir = EndPoint - GetActorLocation();
	float Dist = Dir.Size();
	if (Dist > 1000.f *DeltaTime)
	{
		FVector NewLocation = GetActorLocation() + FMath::Min(Dist, 4000.f * DeltaTime * Dir) / Dist;
		SetActorLocation(NewLocation);
	}
}

void AUTFlagReturnTrail::Destroyed()
{
	Super::Destroyed();
	PSC->DeactivateSystem();
}

void AUTFlagReturnTrail::OnReceivedTeamIndex()
{
	if (PSC)
	{
		PSC->SetColorParameter(FName(TEXT("Color")), (TeamIndex == 1) ? FColor::Blue : FColor::Red);
	}
}

void AUTFlagReturnTrail::SetTeamIndex(uint8 NewValue)
{
	TeamIndex = NewValue;
	OnReceivedTeamIndex();
}