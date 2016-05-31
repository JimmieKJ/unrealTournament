// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTFlagReturnTrail.h"
#include "UTGhostFlag.h"
#include "Particles/ParticleSystemComponent.h"

AUTFlagReturnTrail::AUTFlagReturnTrail(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	InitialLifeSpan = 0.f;
	DedicatedServerLifeSpan = 0.1f; 
	TeamIndex = 255;
	TargetMidPoint = 0;

	PSC->bAutoActivate = true;
	PSC->bAutoDestroy = false;

	static ConstructorHelpers::FObjectFinder<UParticleSystem> TrailEffect(TEXT("ParticleSystem'/Game/RestrictedAssets/Effects/CTF/Particles/PS_GhostFlagConn.PS_GhostFlagConn'"));
	PSC->SetTemplate(TrailEffect.Object);

	MovementSpeed = 8000.f;
}

void AUTFlagReturnTrail::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector EndLocation = (TargetMidPoint < MidPoints.Num()) ? MidPoints[TargetMidPoint] : EndPoint;
	FVector Dir = EndLocation - GetActorLocation();
	float Dist = Dir.Size();
	if (Dist > FMath::Max(50.f, 1000.f*DeltaTime))
	{
		SetActorLocation(GetActorLocation() + FMath::Min(Dist, MovementSpeed * DeltaTime) * Dir / Dist);
	}
	else if (TargetMidPoint < MidPoints.Num())
	{
		TargetMidPoint++;
	}

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

void AUTFlagReturnTrail::EndTrail()
{
	Destroy();
}