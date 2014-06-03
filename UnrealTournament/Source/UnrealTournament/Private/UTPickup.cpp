// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UnrealNetwork.h"

static FName NAME_PercentComplete(TEXT("PercentComplete"));

AUTPickup::AUTPickup(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bCanBeDamaged = false;

	Collision = PCIP.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Capsule"));
	Collision->SetCollisionProfileName(FName(TEXT("Pickup")));
	Collision->InitCapsuleSize(64.0f, 75.0f);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AUTPickup::OnOverlapBegin);
	RootComponent = Collision;

	// can't - not exposed
	/*TimerSprite = PCIP.CreateDefaultSubobject<UMaterialBillboardComponent>(this, TEXT("TimerSprite"));
	if (TimerSprite != NULL)
	{
		TimerSprite->Elements.AddZeroed(1);
		TimerSprite->Elements[0].BaseSizeX = 16.0f;
		TimerSprite->Elements[0].BaseSizeY = 16.0f;
		TimerSprite->SetHiddenInGame(true);
		TimerSprite->AttachParent = RootComponent;
	}*/
	TimerText = PCIP.CreateDefaultSubobject<UTextRenderComponent>(this, TEXT("TimerText"));
	if (TimerText != NULL)
	{
		TimerText->Text = TEXT("30");
		TimerText->SetHiddenInGame(true);
		TimerText->AttachParent = (TimerSprite != NULL) ? TimerSprite : RootComponent;
		TimerText->LDMaxDrawDistance = 1024.0f;
	}

	RespawnTime = 30.0f;
	bDisplayRespawnTimer = true;

	SetReplicates(true);
	bAlwaysRelevant = true;

	PrimaryActorTick.bCanEverTick = true;
}

void AUTPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (GetWorld()->IsGameWorld())
	{
		if (TimerSprite != NULL && TimerSprite->Elements.Num() > 0)
		{
			if (TimerMI == NULL)
			{
				TimerMI = UMaterialInstanceDynamic::Create(TimerSprite->Elements[0].Material, GetWorld());
			}
			TimerSprite->Elements[0].Material = TimerMI;
			TimerSprite->SetCullDistance(TimerText->LDMaxDrawDistance);
		}
	}
}

void AUTPickup::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	APawn* P = Cast<APawn>(OtherActor);
	if (P != NULL && !P->bTearOff)
	{
		ProcessTouch(P);
	}
}

void AUTPickup::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (Role == ROLE_Authority && TouchedBy->Controller != NULL)
	{
		GiveTo(TouchedBy);
		PlayTakenEffects(true);
		StartSleeping();
	}
}

void AUTPickup::GiveTo_Implementation(APawn* Target)
{}

void AUTPickup::SetPickupHidden(bool bNowHidden)
{
	SetActorHiddenInGame(bNowHidden);
}

void AUTPickup::StartSleeping_Implementation()
{
	SetPickupHidden(true);
	SetActorEnableCollision(false);
	if (RespawnTime > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(this, &AUTPickup::WakeUp, RespawnTime, false);
		if (TimerSprite != NULL && TimerSprite->Elements.Num() > 0)
		{
			if (TimerMI != NULL)
			{
				TimerMI->SetScalarParameterValue(NAME_PercentComplete, 0.0f);
			}
			TimerSprite->SetHiddenInGame(false);
		}
		if (TimerText != NULL)
		{
			//TimerText->SetText(FString::Printf(TEXT("%i"), int32(RespawnTime))); // FIXME: not exposed in dll
			TimerText->Text = FString::Printf(TEXT("%i"), int32(RespawnTime));
			TimerText->SetHiddenInGame(false);
		}
		if (TimerSprite != NULL || TimerText != NULL)
		{
			PrimaryActorTick.SetTickFunctionEnable(true);
		}
	}

	if (Role == ROLE_Authority)
	{
		bActive = false;
	}
}
void AUTPickup::PlayTakenEffects(bool bReplicate)
{
	if (bReplicate)
	{
		bRepTakenEffects = true;
	}
	// TODO: EffectIsRelevant() ?
	if (GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnEmitterAttached(TakenParticles, RootComponent);
		UUTGameplayStatics::UTPlaySound(GetWorld(), TakenSound, this, SRT_None);
	}
}
void AUTPickup::ReplicatedTakenEffects()
{
	if (bRepTakenEffects)
	{
		PlayTakenEffects(false);
	}
}
void AUTPickup::WakeUp_Implementation()
{
	SetPickupHidden(false);
	SetActorEnableCollision(true);
	GetWorld()->GetTimerManager().ClearTimer(this, &AUTPickup::WakeUp);

	PrimaryActorTick.SetTickFunctionEnable(GetClass()->GetDefaultObject<AUTPickup>()->PrimaryActorTick.bStartWithTickEnabled);
	if (TimerSprite != NULL)
	{
		TimerSprite->SetHiddenInGame(true);
	}
	if (TimerText != NULL)
	{
		TimerText->SetHiddenInGame(true);
	}

	if (Role == ROLE_Authority)
	{
		bActive = true;
		bRepTakenEffects = false;
	}
}
void AUTPickup::PlayRespawnEffects()
{
	// TODO: EffectIsRelevant() ?
	if (GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnEmitterAttached(RespawnParticles, RootComponent);
		UUTGameplayStatics::UTPlaySound(GetWorld(), RespawnSound, this, SRT_None);
	}
}

void AUTPickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (RespawnTime > 0.0f && !bActive)
	{
		if (TimerMI != NULL)
		{
			TimerMI->SetScalarParameterValue(NAME_PercentComplete, 1.0f - GetWorld()->GetTimerManager().GetTimerRemaining(this, &AUTPickup::WakeUp) / RespawnTime);
		}
		if (TimerText != NULL)
		{
			FString NewText = FString::Printf(TEXT("%i"), int32(GetWorld()->GetTimerManager().GetTimerRemaining(this, &AUTPickup::WakeUp)));
			if (NewText != TimerText->Text)
			{
				//TimerText->SetText(NewText); // FIXME: not exposed in dll
				TimerText->Text = NewText;
				TimerText->MarkRenderStateDirty();
			}
		}
	}
}

void AUTPickup::OnRep_bActive()
{
	if (bActive)
	{
		WakeUp();
	}
	else
	{
		StartSleeping();
	}
}

void AUTPickup::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTPickup, bActive, COND_None);
}
