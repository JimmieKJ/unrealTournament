// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickupCoin.h"
#include "UTKMGameMode.h"


AUTPickupCoin::AUTPickupCoin(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	InitialLifeSpan=0.0;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CrownMesh(TEXT("StaticMesh'/Game/RestrictedAssets/Meshes/SM_UT4_Logo.SM_UT4_Logo'"));

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

void AUTPickupCoin::SetValue(float NewValue)
{
	Value = NewValue;
	if (Value >= 500) Mesh->SetWorldScale3D(FVector(1.5, 1.5, 1.5f));
	else if (Value >= 100) Mesh->SetWorldScale3D(FVector(1.0, 1.0, 1.0f));
	else Mesh->SetWorldScale3D(FVector(0.75, 0.75, 0.75f));
}

void AUTPickupCoin::GiveTo_Implementation(APawn* Target)
{
	if (Target->bTearOff) return;	// Don't give to dead pawns.

	UE_LOG(UT,Log,TEXT("GiveTo"))
	AUTPlayerState* PS = Cast<AUTPlayerState>(Target->PlayerState);
	if (PS)
	{
		PS->AdjustCurrency(Value);
	}

	Destroy();
}

