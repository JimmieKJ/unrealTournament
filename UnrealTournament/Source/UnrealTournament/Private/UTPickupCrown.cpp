// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickupCrown.h"
#include "UTKMGameMode.h"


AUTPickupCrown::AUTPickupCrown(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	InitialLifeSpan=0.0;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CrownMesh(TEXT("StaticMesh'/Game/RestrictedAssets/Pickups/Decos/crown.crown'"));

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
	StaticMesh->SetWorldScale3D(FVector(2.0f,2.0f,2.0f));
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
	}
}