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
	bReachedMidPoint = false;

	PSC->bAutoActivate = true;
	PSC->bAutoDestroy = false;

	static ConstructorHelpers::FObjectFinder<UParticleSystem> TrailEffect(TEXT("ParticleSystem'/Game/RestrictedAssets/Effects/CTF/Particles/PS_GhostFlagConn.PS_GhostFlagConn'"));
	PSC->SetTemplate(TrailEffect.Object);

	MovementSpeed = 8000.f;
}

void AUTFlagReturnTrail::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	FVector StartLocation = (StartActor && (StartPoint != StartActor->GetActorLocation())) ? StartActor->GetActorLocation() : GetActorLocation();
	FVector Dir = EndPoint - StartLocation;
	bool bOldReach = bReachedMidPoint;
	if (!bReachedMidPoint)
	{
		Dir = MidPoint - StartLocation;
		if (Dir.Size() <= FMath::Max(1000.f *DeltaTime, 50.f))
		{
			Dir = EndPoint - StartLocation;
			bReachedMidPoint = true;
		}
	}

	float Dist = Dir.Size();
	if (Dist > 1000.f *DeltaTime)
	{
		FVector NewLocation = StartLocation + FMath::Min(Dist, MovementSpeed * DeltaTime) * Dir / Dist;
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

void AUTFlagReturnTrail::EndTrail()
{
	Destroy();
}