// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFFlag.h"
#include "UTCharacter.h"
#include "Net/UnrealNetwork.h"
#include "UTRallyPoint.h"

AUTRallyPoint::AUTRallyPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(ACharacter::CapsuleComponentName);
	Capsule->SetCollisionProfileName(FName(TEXT("Pickup")));
	Capsule->InitCapsuleSize(92.f, 134.0f);
	//Capsule->bShouldUpdatePhysicsVolume = false;
	Capsule->Mobility = EComponentMobility::Static;
	Capsule->OnComponentBeginOverlap.AddDynamic(this, &AUTRallyPoint::OnOverlapBegin);
	RootComponent = Capsule;

	PrimaryActorTick.bCanEverTick = true;
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

void AUTRallyPoint::FlagCarrierInVolume(AUTCharacter* NewFC)
{
	NearbyFC = NewFC;
	bShowAvailableEffect = (NearbyFC != nullptr);
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

void AUTRallyPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowAvailableEffect && (Role == ROLE_Authority))
	{
		if (!NearbyFC || NearbyFC->IsPendingKillPending() || !NearbyFC->GetCarriedObject())
		{
			UE_LOG(UT, Warning, TEXT("FAILSAFE CLEAR RALLY POINTS"));
			FlagCarrierInVolume(nullptr);
		}
	}
}

// decal on ground
// effect when FC touching scales up to full rally enabled in 2 seconds
// team based effects

