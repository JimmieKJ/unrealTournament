// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTAvoidMarker.h"
#include "UTBot.h"

AUTAvoidMarker::AUTAvoidMarker(const FObjectInitializer& OI)
: Super(OI)
{
	Capsule = OI.CreateOptionalDefaultSubobject<UCapsuleComponent>(this, FName(TEXT("Capsule")));
	if (Capsule != NULL)
	{
		Capsule->SetCapsuleSize(225.0f, 100.0f);
		Capsule->SetCollisionProfileName(FName(TEXT("ProjectileOverlap")));
		Capsule->OnComponentBeginOverlap.AddDynamic(this, &AUTAvoidMarker::OnOverlapBegin);
		RootComponent = Capsule;
	}

	TeamNum = 255;
}

void AUTAvoidMarker::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() == NULL)
	{
		UE_LOG(UT, Warning, TEXT("UTAvoidMarker requires an owner"));
		Destroy();
	}
}

void AUTAvoidMarker::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (GetOwner() == NULL)
	{
		// owner expired and didn't clean us up
		Destroy();
	}
	else
	{
		APawn* P = Cast<APawn>(OtherActor);
		if (P != NULL)
		{
			AUTBot* B = Cast<AUTBot>(P->Controller);
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (B != NULL && (bFriendlyAvoid || GS == NULL || !GS->OnSameTeam(B, this)))
			{
				B->AddFearSpot(this);
			}
		}
	}
}
