// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickupCrown.h"
#include "UTKMGameMode.h"

static FName NAME_PercentComplete(TEXT("PercentComplete"));


AUTPickupCrown::AUTPickupCrown(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	InitialLifeSpan=0.0;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CrownMesh(TEXT("StaticMesh'/Game/EpicInternal/PK/crown.crown'"));

	Collision->InitCapsuleSize(48, 48.0f);
	UStaticMeshComponent* StaticMesh = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, FName(TEXT("Mesh")));
	StaticMesh->AttachParent = RootComponent;
	StaticMesh->AlwaysLoadOnClient = true;
	StaticMesh->AlwaysLoadOnServer = true;
	StaticMesh->bCastDynamicShadow = true;
	StaticMesh->bAffectDynamicIndirectLighting = true;
	StaticMesh->PrimaryComponentTick.TickGroup = TG_PrePhysics;
	StaticMesh->SetCollisionProfileName(FName(TEXT("NoCollision")));
	StaticMesh->bGenerateOverlapEvents = false;
	StaticMesh->bCanEverAffectNavigation = false;
	StaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMesh->bReceivesDecals = false;
	StaticMesh->SetStaticMesh(CrownMesh.Object);
	Mesh = StaticMesh;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;

}

void AUTPickupCrown::GiveTo_Implementation(APawn* Target)
{
	UE_LOG(UT,Log,TEXT("GiveTo"))
	AUTPlayerState* PS = Cast<AUTPlayerState>(Target->PlayerState);
	if (PS)
	{
		UE_LOG(UT,Log,TEXT("PS"));
		AUTKMGameMode* KingGame = GetWorld()->GetAuthGameMode<AUTKMGameMode>();
		if (KingGame)
		{
			UE_LOG(UT,Log,TEXT("Becoming King"));
			KingGame->BecomeKing(PS);
		}
	}

	Destroy();
}

void AUTPickupCrown::Destroyed()
{
	Super::Destroyed();
	UE_LOG(UT,Log,TEXT("Pickup Crown Destroyed"));
}


void AUTPickupCrown::Tick( float DeltaSeconds )
{
	Super::Tick(DeltaSeconds);

	FString TouchingActors;
	if (Collision)
	{
		TArray<UPrimitiveComponent*> Overlaps;
		Collision->GetOverlappingComponents(Overlaps);
		if (Overlaps.Num() > 0)
		{
			for (int i=0; i < Overlaps.Num(); i++)
			{
				TouchingActors += i == 0 ? Overlaps[i]->GetFullName() : TEXT(", ") + Overlaps[i]->GetFullName();
			}
		}
		else
		{
			TouchingActors = TEXT("None");
		}
		UE_LOG(UT,Log,TEXT("Collision: %s %i"), *TouchingActors, Collision->IsPendingKill());
	}
	else
	{
		UE_LOG(UT,Log,TEXT("Collision: No Collision Component"));
	}
}