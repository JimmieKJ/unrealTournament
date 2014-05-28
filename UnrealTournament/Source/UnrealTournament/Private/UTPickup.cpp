// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UnrealNetwork.h"

AUTPickup::AUTPickup(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bCanBeDamaged = false;

	Collision = PCIP.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Capsule"));
	Collision->InitCapsuleSize(64.0f, 75.0f);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AUTPickup::OnOverlapBegin);
	RootComponent = Collision;

	RespawnTime = 30.0f;

	SetReplicates(true);
	bAlwaysRelevant = true;
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
		StartSleeping();
	}
}

void AUTPickup::GiveTo_Implementation(APawn* Target)
{}

void AUTPickup::StartSleeping_Implementation()
{
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	if (RespawnTime > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(this, &AUTPickup::WakeUp, RespawnTime, false);
	}

	if (Role == ROLE_Authority)
	{
		bActive = false;
	}
}
void AUTPickup::PlayTakenEffects()
{
	// TODO: EffectIsRelevant() ?
	if (GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnEmitterAttached(TakenParticles, RootComponent);
		UUTGameplayStatics::UTPlaySound(GetWorld(), TakenSound, this, SRT_None);
	}
}
void AUTPickup::WakeUp_Implementation()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	GetWorld()->GetTimerManager().ClearTimer(this, &AUTPickup::WakeUp);

	if (Role == ROLE_Authority)
	{
		bActive = true;
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
