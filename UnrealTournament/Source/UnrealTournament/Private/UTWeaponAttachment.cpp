// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeaponAttachment.h"

AUTWeaponAttachment::AUTWeaponAttachment(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	RootComponent = PCIP.CreateDefaultSubobject<USceneComponent, USceneComponent>(this, TEXT("DummyRoot"), false, false, false);
	Mesh = PCIP.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("Mesh3P"));
	Mesh->SetOwnerNoSee(true);
	Mesh->AttachParent = RootComponent;

	for (int32 i = 0; i < 2; i++)
	{
		UParticleSystemComponent* PSC = PCIP.CreateDefaultSubobject<UParticleSystemComponent, UParticleSystemComponent>(this, FName(*FString::Printf(TEXT("MuzzleFlash%i"), i)), false, false, false);
		MuzzleFlash.Add(PSC);
		if (PSC != NULL)
		{
			PSC->bAutoActivate = false;
			PSC->SetOwnerNoSee(true);
			PSC->AttachParent = Mesh;
		}
	}
}

void AUTWeaponAttachment::BeginPlay()
{
	Super::BeginPlay();

	UTOwner = Cast<AUTCharacter>(Instigator);
	if (UTOwner != NULL)
	{
		WeaponType = UTOwner->GetWeaponClass();
		AttachToOwner();
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("UTWeaponAttachment: Bad Instigator: %s"), *GetNameSafe(Instigator));
	}
}

void AUTWeaponAttachment::Destroyed()
{
	DetachFromOwner();

	Super::Destroyed();
}

void AUTWeaponAttachment::AttachToOwner_Implementation()
{
	Mesh->SetRelativeLocation(AttachOffset);
	Mesh->AttachTo(UTOwner->Mesh, AttachSocket);
}

void AUTWeaponAttachment::DetachFromOwner_Implementation()
{
	Mesh->DetachFromParent();
}

void AUTWeaponAttachment::PlayFiringEffects()
{
	// muzzle flash
	if (MuzzleFlash.IsValidIndex(UTOwner->FireMode) && MuzzleFlash[UTOwner->FireMode] != NULL)
	{
		MuzzleFlash[UTOwner->FireMode]->ActivateSystem();
	}

	// fire effects
	if (FireEffect.IsValidIndex(UTOwner->FireMode) && FireEffect[UTOwner->FireMode] != NULL)
	{
		const FVector SpawnLocation = (MuzzleFlash.IsValidIndex(UTOwner->FireMode) && MuzzleFlash[UTOwner->FireMode] != NULL) ? MuzzleFlash[UTOwner->FireMode]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetActorRotation().RotateVector(FVector(UTOwner->GetSimpleCollisionCylinderExtent().X, 0.0f, 0.0f));
		UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FireEffect[UTOwner->FireMode], SpawnLocation, (UTOwner->FlashLocation - SpawnLocation).Rotation(), true);
		static FName NAME_HitLocation(TEXT("HitLocation"));
		PSC->SetVectorParameter(NAME_HitLocation, UTOwner->FlashLocation);
	}
}

void AUTWeaponAttachment::StopFiringEffects()
{

}