// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeaponAttachment.h"
#include "Particles/ParticleSystemComponent.h"

AUTWeaponAttachment::AUTWeaponAttachment(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	RootComponent = PCIP.CreateDefaultSubobject<USceneComponent, USceneComponent>(this, TEXT("DummyRoot"), false, false, false);
	Mesh = PCIP.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("Mesh3P"));
	Mesh->SetOwnerNoSee(true);
	Mesh->AttachParent = RootComponent;
	Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	AttachSocket = FName((TEXT("WeaponPoint")));

	PickupScaleOverride = FVector(2.0f, 2.0f, 2.0f);
}

void AUTWeaponAttachment::BeginPlay()
{
	Super::BeginPlay();

	AUTWeapon::InstanceMuzzleFlashArray(this, MuzzleFlash);

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

void AUTWeaponAttachment::RegisterAllComponents()
{
	// sanity check some settings
	for (int32 i = 0; i < MuzzleFlash.Num(); i++)
	{
		if (MuzzleFlash[i] != NULL)
		{
			MuzzleFlash[i]->bAutoActivate = false;
			MuzzleFlash[i]->SetOwnerNoSee(true);
		}
	}
	Super::RegisterAllComponents();
}

void AUTWeaponAttachment::AttachToOwner_Implementation()
{
	Mesh->SetRelativeLocation(AttachOffset);
	Mesh->AttachTo(UTOwner->Mesh, AttachSocket);
	Mesh->bRecentlyRendered = UTOwner->Mesh->bRecentlyRendered;
	Mesh->LastRenderTime = UTOwner->Mesh->LastRenderTime;
	UpdateOverlays();
	SetSkin(UTOwner->GetSkin());
}

void AUTWeaponAttachment::DetachFromOwner_Implementation()
{
	Mesh->DetachFromParent();
}

void AUTWeaponAttachment::UpdateOverlays()
{
	WeaponType.GetDefaultObject()->UpdateOverlaysShared(this, UTOwner, Mesh, OverlayMesh);
}

void AUTWeaponAttachment::SetSkin(UMaterialInterface* NewSkin)
{
	if (NewSkin != NULL)
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			Mesh->SetMaterial(i, NewSkin);
		}
	}
	else
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			Mesh->SetMaterial(i, GetClass()->GetDefaultObject<AUTWeaponAttachment>()->Mesh->GetMaterial(i));
		}
	}
}

void AUTWeaponAttachment::PlayFiringEffects()
{
	// stop any firing effects for other firemodes
	// this is needed because if the user swaps firemodes very quickly replication might not receive a discrete stop and start new
	StopFiringEffects(true);

	// muzzle flash
	if (MuzzleFlash.IsValidIndex(UTOwner->FireMode) && MuzzleFlash[UTOwner->FireMode] != NULL && MuzzleFlash[UTOwner->FireMode]->Template != NULL)
	{
		// if we detect a looping particle system, then don't reactivate it
		if ( !MuzzleFlash[UTOwner->FireMode]->bIsActive || MuzzleFlash[UTOwner->FireMode]->Template->Emitters[0] == NULL ||
			IsLoopingParticleSystem(MuzzleFlash[UTOwner->FireMode]->Template) )
		{
			MuzzleFlash[UTOwner->FireMode]->ActivateSystem();
		}
	}

	// fire effects
	static FName NAME_HitLocation(TEXT("HitLocation"));
	static FName NAME_LocalHitLocation(TEXT("LocalHitLocation"));
	if (FireEffect.IsValidIndex(UTOwner->FireMode) && FireEffect[UTOwner->FireMode] != NULL)
	{
		const FVector SpawnLocation = (MuzzleFlash.IsValidIndex(UTOwner->FireMode) && MuzzleFlash[UTOwner->FireMode] != NULL) ? MuzzleFlash[UTOwner->FireMode]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetActorRotation().RotateVector(FVector(UTOwner->GetSimpleCollisionCylinderExtent().X, 0.0f, 0.0f));
		UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FireEffect[UTOwner->FireMode], SpawnLocation, (UTOwner->FlashLocation - SpawnLocation).Rotation(), true);
		PSC->SetVectorParameter(NAME_HitLocation, UTOwner->FlashLocation);
		PSC->SetVectorParameter(NAME_LocalHitLocation, PSC->ComponentToWorld.InverseTransformPosition(UTOwner->FlashLocation));
	}
	// perhaps the muzzle flash also contains hit effect (constant beam, etc) so set the parameter on it instead
	else if (MuzzleFlash.IsValidIndex(UTOwner->FireMode) && MuzzleFlash[UTOwner->FireMode] != NULL)
	{
		MuzzleFlash[UTOwner->FireMode]->SetVectorParameter(NAME_HitLocation, UTOwner->FlashLocation);
		MuzzleFlash[UTOwner->FireMode]->SetVectorParameter(NAME_LocalHitLocation, MuzzleFlash[UTOwner->FireMode]->ComponentToWorld.InverseTransformPosition(UTOwner->FlashLocation));
	}
}

void AUTWeaponAttachment::StopFiringEffects(bool bIgnoreCurrentMode)
{
	// we need to default to stopping all modes' firing effects as we can't rely on the replicated value to be correct at this point
	for (uint8 i = 0; i < MuzzleFlash.Num(); i++)
	{
		if (MuzzleFlash[i] != NULL && (!bIgnoreCurrentMode || i != UTOwner->FireMode))
		{
			MuzzleFlash[i]->DeactivateSystem();
		}
	}
}