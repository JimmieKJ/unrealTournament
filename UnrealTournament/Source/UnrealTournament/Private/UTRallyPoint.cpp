// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFFlag.h"
#include "Net/UnrealNetwork.h"
#include "UTRallyPoint.h"

AUTRallyPoint::AUTRallyPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Capsule = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Capsule"));

	// overlap Pawns, no other collision
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->InitCapsuleSize(92.f, 134.0f);
	Capsule->OnComponentBeginOverlap.AddDynamic(this, &AUTRallyPoint::OnOverlapBegin);
	Capsule->SetupAttachment(RootComponent);
}

void AUTRallyPoint::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTRallyPoint, bShowAvailableEffect);
}

void AUTRallyPoint::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AUTCharacter* Character = Cast<AUTCharacter>(OtherActor);
	if (Role == ROLE_Authority && Character)
	{
		AUTCTFFlag* CharFlag = Cast<AUTCTFFlag>(Character->GetCarriedObject());
		if (CharFlag != NULL)
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS == NULL || (GS->IsMatchInProgress() && !GS->IsMatchIntermission() && !GS->HasMatchEnded()))
			{
				CharFlag->PlayCaptureEffect();
			}
		}
	}
}

void AUTRallyPoint::BeginPlay()
{
	Super::BeginPlay();

	// associate as team locker with team volume I am in
	TArray<UPrimitiveComponent*> OverlappingComponents;
	Capsule->GetOverlappingComponents(OverlappingComponents);
	MyGameVolume = nullptr;
	int32 BestPriority = -1.f;

	for (auto CompIt = OverlappingComponents.CreateIterator(); CompIt; ++CompIt)
	{
		UPrimitiveComponent* OtherComponent = *CompIt;
		if (OtherComponent && OtherComponent->bGenerateOverlapEvents)
		{
			AUTGameVolume* V = Cast<AUTGameVolume>(OtherComponent->GetOwner());
			if (V && V->Priority > BestPriority)
			{
				if (V->IsOverlapInVolume(*Capsule))
				{
					BestPriority = V->Priority;
					MyGameVolume = V;
				}
			}
		}
	}
	if (MyGameVolume)
	{
		MyGameVolume->RallyPoints.AddUnique(this);
	}
}

void AUTRallyPoint::FlagCarrierInVolume(bool bFCInVolume)
{
	bShowAvailableEffect = bFCInVolume;
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnAvailableEffectChanged();
	}
}

void AUTRallyPoint::OnAvailableEffectChanged()
{
	if (AvailableEffectPSC != nullptr)
	{
		// clear it
		AvailableEffectPSC->ActivateSystem(false);
		AvailableEffectPSC->UnregisterComponent();
		AvailableEffectPSC = nullptr;
	}
	if (bShowAvailableEffect)
	{
		AvailableEffectPSC = UGameplayStatics::SpawnEmitterAtLocation(this, AvailableEffect, GetActorLocation() + FVector(0.f, 0.f, 80.f), GetActorRotation());
	}
}


// decal on ground
// effect that shows up when FC nearby
// effect when FC touching scales up to full rally enabled in 2 seconds
// team based effects

