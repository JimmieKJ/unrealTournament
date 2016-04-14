// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTFlagReturnTrail.h"
#include "Particles/ParticleSystemComponent.h"

AUTFlagReturnTrail::AUTFlagReturnTrail(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	InitialLifeSpan = 10.f;
	DedicatedServerLifeSpan = 0.1f; 
	TeamIndex = 255;

	PSC->bAutoActivate = true;
	PSC->bAutoDestroy = false;

	static ConstructorHelpers::FObjectFinder<UParticleSystem> TrailEffect(TEXT("ParticleSystem'/Game/RestrictedAssets/Effects/CTF/Particles/PS_Flag_Trail.PS_Flag_Trail'"));
	PSC->SetTemplate(TrailEffect.Object);
}

void AUTFlagReturnTrail::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	FVector StartLocation = (StartActor && (StartPoint != StartActor->GetActorLocation())) ? StartActor->GetActorLocation() : GetActorLocation();
	FVector Dir = EndPoint - StartLocation;
	float Dist = Dir.Size();
	if (Dist > 1000.f *DeltaTime)
	{
		FVector NewLocation = StartLocation + FMath::Min(Dist, 8000.f * DeltaTime) * Dir / Dist;
		SetActorLocation(NewLocation);
	}
	StartPoint = StartActor ? StartActor->GetActorLocation() : StartPoint;
}

void AUTFlagReturnTrail::Destroyed()
{
	Super::Destroyed();
	PSC->DeactivateSystem();
}

void AUTFlagReturnTrail::SetTeamIndex(uint8 NewValue)
{
	TeamIndex = NewValue;
	if (PSC)
	{
		PSC->SetColorParameter(FName(TEXT("Color")), (TeamIndex == 1) ? FColor::Blue : FColor::Red);
	}
}