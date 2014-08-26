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

	State.bActive = true;
	RespawnTime = 30.0f;
	bDisplayRespawnTimer = true;

	SetReplicates(true);
	bAlwaysRelevant = true;
	NetUpdateFrequency = 1.0f;

	PrimaryActorTick.bCanEverTick = true;

	PickupType = PC_Minor;
}

void AUTPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

}

void AUTPickup::SetupTimerSprite()
{
	if (GetWorld()->IsGameWorld())
	{
		// due to editor limitations the TimerSprite gets set via the blueprint construction script - initialize its material instance here
		if (TimerSprite != NULL && TimerSprite->Elements.Num() > 0)
		{
			if (TimerMI == NULL)
			{
				TimerMI = UMaterialInstanceDynamic::Create(TimerSprite->Elements[0].Material, GetWorld());
			}
			TimerSprite->Elements[0].Material = TimerMI;
			TimerSprite->LDMaxDrawDistance = 1024.0f;
			TimerSprite->SetHiddenInGame(true);
		}
	}
}

void AUTPickup::BeginPlay()
{
	Super::BeginPlay();

	SetupTimerSprite();

	/*if (Role == ROLE_Authority && bDelayedSpawn)
	{
		StartSleeping();
	}*/
}

void AUTPickup::Reset_Implementation()
{
	if (bDelayedSpawn)
	{
		State.bRepTakenEffects = false;
		StartSleeping();
	}
	else if (!State.bActive)
	{
		WakeUp();
	}
}

void AUTPickup::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	APawn* P = Cast<APawn>(OtherActor);
	if (P != NULL && !P->bTearOff)
	{
		ProcessTouch(P);
	}
}

void AUTPickup::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (Role == ROLE_Authority && State.bActive && TouchedBy->Controller != NULL)
	{
		GiveTo(TouchedBy);
		AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (UTGameMode != NULL)
		{
			AUTPlayerState* PickedUpBy = Cast<AUTPlayerState>(TouchedBy->PlayerState);
			UTGameMode->ScorePickup(this, PickedUpBy, LastPickedUpBy);
			LastPickedUpBy = PickedUpBy;
		}

		PlayTakenEffects(true);
		StartSleeping();
	}
}

void AUTPickup::GiveTo_Implementation(APawn* Target)
{}

void AUTPickup::SetPickupHidden(bool bNowHidden)
{
	if (TakenHideTags.Num() == 0 || RootComponent == NULL)
	{
		SetActorHiddenInGame(bNowHidden);
	}
	else
	{
		TArray<USceneComponent*> Components;
		RootComponent->GetChildrenComponents(true, Components);
		for (int32 i = 0; i < Components.Num(); i++)
		{
			for (int32 j = 0; j < TakenHideTags.Num(); j++)
			{
				if (Components[i]->ComponentHasTag(TakenHideTags[j]))
				{
					Components[i]->SetHiddenInGame(bNowHidden);
				}
			}
		}
	}
}

void AUTPickup::StartSleeping_Implementation()
{
	SetPickupHidden(true);
	SetActorEnableCollision(false);
	if (RespawnTime > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(this, &AUTPickup::WakeUpTimer, RespawnTime, false);
		if (TimerSprite != NULL && TimerSprite->Elements.Num() > 0)
		{
			if (TimerMI != NULL)
			{
				TimerMI->SetScalarParameterValue(NAME_PercentComplete, 0.0f);
			}
			TimerSprite->SetHiddenInGame(false);
		}
		if (TimerSprite != NULL)
		{
			PrimaryActorTick.SetTickFunctionEnable(true);
		}
	}

	if (Role == ROLE_Authority)
	{
		State.bActive = false;
		State.ChangeCounter++;
		ForceNetUpdate();
	}
}
void AUTPickup::PlayTakenEffects(bool bReplicate)
{
	if (bReplicate)
	{
		State.bRepTakenEffects = true;
		ForceNetUpdate();
	}
	// TODO: EffectIsRelevant() ?
	if (GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnEmitterAttached(TakenParticles, RootComponent);
		UUTGameplayStatics::UTPlaySound(GetWorld(), TakenSound, this, SRT_None);
	}
}
void AUTPickup::WakeUp_Implementation()
{
	SetPickupHidden(false);
	GetWorld()->GetTimerManager().ClearTimer(this, &AUTPickup::WakeUpTimer);

	PrimaryActorTick.SetTickFunctionEnable(GetClass()->GetDefaultObject<AUTPickup>()->PrimaryActorTick.bStartWithTickEnabled);
	if (TimerSprite != NULL)
	{
		TimerSprite->SetHiddenInGame(true);
	}

	if (Role == ROLE_Authority)
	{
		State.bActive = true;
		State.bRepTakenEffects = false;
		State.ChangeCounter++;
		ForceNetUpdate();
	}

	PlayRespawnEffects();

	// last so if a player is already touching we're fully ready to act on it
	SetActorEnableCollision(true);
}
void AUTPickup::WakeUpTimer()
{
	if (Role == ROLE_Authority)
	{
		WakeUp();
	}
	else
	{
		// it's possible we're out of sync, so set up a state that indicates the pickup should respawn any time now, but isn't yet available
		if (TimerMI != NULL)
		{
			TimerMI->SetScalarParameterValue(NAME_PercentComplete, 0.99f);
		}
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

	UWorld* World = GetWorld();
	if (RespawnTime > 0.0f && !State.bActive && World->GetTimerManager().IsTimerActive(this, &AUTPickup::WakeUpTimer))
	{
		if (TimerMI != NULL)
		{
			TimerMI->SetScalarParameterValue(NAME_PercentComplete, 1.0f - World->GetTimerManager().GetTimerRemaining(this, &AUTPickup::WakeUpTimer) / RespawnTime);
		}
	}
}

static FPickupReplicatedState PreRepState;

void AUTPickup::PreNetReceive()
{
	PreRepState = State;
	Super::PreNetReceive();
}
void AUTPickup::PostNetReceive()
{
	Super::PostNetReceive();

	// make sure not to re-invoke WakeUp()/StartSleeping() if only bRepTakenEffects has changed
	// since that will reset timers on the client incorrectly
	if (PreRepState.bActive != State.bActive || PreRepState.ChangeCounter != State.ChangeCounter)
	{
		if (State.bActive)
		{
			WakeUp();
		}
		else
		{
			StartSleeping();
		}
	}
	if (!State.bActive && State.bRepTakenEffects)
	{
		PlayTakenEffects(false);
	}
}

void AUTPickup::OnRep_RespawnTimeRemaining()
{
	if (!State.bActive)
	{
		GetWorld()->GetTimerManager().SetTimer(this, &AUTPickup::WakeUpTimer, RespawnTimeRemaining, false);
	}
}

void AUTPickup::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	RespawnTimeRemaining = GetWorld()->GetTimerManager().GetTimerRemaining(this, &AUTPickup::WakeUpTimer);
}

void AUTPickup::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// warning: we rely on this ordering
	DOREPLIFETIME_CONDITION(AUTPickup, State, COND_None);
	DOREPLIFETIME_CONDITION(AUTPickup, RespawnTimeRemaining, COND_InitialOnly);
}
