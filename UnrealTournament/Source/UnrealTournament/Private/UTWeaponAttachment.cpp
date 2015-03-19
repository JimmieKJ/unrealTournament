// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeaponAttachment.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTImpactEffect.h"
#include "UTWorldSettings.h"

AUTWeaponAttachment::AUTWeaponAttachment(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent, USceneComponent>(this, TEXT("DummyRoot"), false);
	Mesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("Mesh3P"));
	Mesh->AttachParent = RootComponent;
	Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	Mesh->bLightAttachmentsAsGroup = true;
	Mesh->bReceivesDecals = false;
	AttachSocket = FName((TEXT("WeaponPoint")));
	HolsterSocket = FName((TEXT("spine_02")));
	HolsterOffset = FVector(0.f, 16.f, 0.f);
	HolsterRotation = FRotator(0.f, 60.f, 75.f);
	PickupScaleOverride = FVector(2.0f, 2.0f, 2.0f);
	WeaponStance = 0;

	bCopyWeaponImpactEffect = true;

	MaxBulletWhipDist = 200.0f;
}

void AUTWeaponAttachment::BeginPlay()
{
	Super::BeginPlay();

	AUTWeapon::InstanceMuzzleFlashArray(this, MuzzleFlash);

	UTOwner = Cast<AUTCharacter>(Instigator);
	if (UTOwner != NULL)
	{
		WeaponType = UTOwner->GetWeaponClass();
		if (WeaponType != NULL && bCopyWeaponImpactEffect && ImpactEffect.Num() == 0)
		{
			ImpactEffect = WeaponType.GetDefaultObject()->ImpactEffect;
			ImpactEffectSkipDistance = WeaponType.GetDefaultObject()->ImpactEffectSkipDistance;
			MaxImpactEffectSkipTime = WeaponType.GetDefaultObject()->MaxImpactEffectSkipTime;
		}
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
	GetWorldTimerManager().ClearAllTimersForObject(this);
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
			MuzzleFlash[i]->SetOwnerNoSee(false);
		}
	}
	Super::RegisterAllComponents();
}

void AUTWeaponAttachment::AttachToOwner_Implementation()
{
	AttachToOwnerNative();
}

void AUTWeaponAttachment::AttachToOwnerNative()
{
	Mesh->AttachTo(UTOwner->GetMesh(), AttachSocket);
	Mesh->SetRelativeLocation(AttachOffset);
	Mesh->bRecentlyRendered = UTOwner->GetMesh()->bRecentlyRendered;
	Mesh->LastRenderTime = UTOwner->GetMesh()->LastRenderTime;
	UpdateOverlays();
	SetSkin(UTOwner->GetSkin());
}

void AUTWeaponAttachment::HolsterToOwner_Implementation()
{
	HolsterToOwnerNative();
}

void AUTWeaponAttachment::HolsterToOwnerNative()
{
	Mesh->AttachTo(UTOwner->GetMesh(), HolsterSocket);
	Mesh->SetRelativeLocation(HolsterOffset);
	Mesh->SetRelativeRotation(HolsterRotation);
	Mesh->bRecentlyRendered = UTOwner->GetMesh()->bRecentlyRendered;
	Mesh->LastRenderTime = UTOwner->GetMesh()->LastRenderTime;
	SetSkin(UTOwner->GetSkin());
}

void AUTWeaponAttachment::DetachFromOwner_Implementation()
{
	Mesh->DetachFromParent();
}

void AUTWeaponAttachment::UpdateOverlays()
{
	if (WeaponType != NULL)
	{
		WeaponType.GetDefaultObject()->UpdateOverlaysShared(this, UTOwner, Mesh, OverlayMesh);
	}
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
		if (!MuzzleFlash[UTOwner->FireMode]->bIsActive || MuzzleFlash[UTOwner->FireMode]->bSuppressSpawning || !IsLoopingParticleSystem(MuzzleFlash[UTOwner->FireMode]->Template))
		{
			MuzzleFlash[UTOwner->FireMode]->ActivateSystem();
		}
	}

	// fire effects
	static FName NAME_HitLocation(TEXT("HitLocation"));
	static FName NAME_LocalHitLocation(TEXT("LocalHitLocation"));
	const FVector SpawnLocation = (MuzzleFlash.IsValidIndex(UTOwner->FireMode) && MuzzleFlash[UTOwner->FireMode] != NULL) ? MuzzleFlash[UTOwner->FireMode]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetActorRotation().RotateVector(FVector(UTOwner->GetSimpleCollisionCylinderExtent().X, 0.0f, 0.0f));
	if (FireEffect.IsValidIndex(UTOwner->FireMode) && FireEffect[UTOwner->FireMode] != NULL)
	{
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

	if ((UTOwner->FlashLocation - LastImpactEffectLocation).Size() >= ImpactEffectSkipDistance || GetWorld()->TimeSeconds - LastImpactEffectTime >= MaxImpactEffectSkipTime)
	{
		if (ImpactEffect.IsValidIndex(UTOwner->FireMode) && ImpactEffect[UTOwner->FireMode] != NULL)
		{
			FHitResult ImpactHit = AUTWeapon::GetImpactEffectHit(UTOwner, SpawnLocation, UTOwner->FlashLocation);
			ImpactEffect[UTOwner->FireMode].GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(ImpactHit.Normal.Rotation(), ImpactHit.Location), ImpactHit.Component.Get(), NULL, UTOwner->Controller);
		}
		LastImpactEffectLocation = UTOwner->FlashLocation;
		LastImpactEffectTime = GetWorld()->TimeSeconds;
	}

	PlayBulletWhip();
}

void AUTWeaponAttachment::PlayBulletWhip()
{
	if (BulletWhip != NULL && !UTOwner->FlashLocation.IsZero())
	{
		const FVector BulletSrc = UTOwner->GetActorLocation();
		const FVector Dir = (UTOwner->FlashLocation - BulletSrc).GetSafeNormal();
		for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
			if (PC != NULL && PC->GetViewTarget() != UTOwner)
			{
				FVector ViewLoc;
				FRotator ViewRot;
				PC->GetPlayerViewPoint(ViewLoc, ViewRot);
				const FVector ClosestPt = FMath::ClosestPointOnSegment(ViewLoc, BulletSrc, UTOwner->FlashLocation);
				if (ClosestPt != BulletSrc && ClosestPt != UTOwner->FlashLocation && (ClosestPt - ViewLoc).Size() <= MaxBulletWhipDist)
				{
					// trace to make sure missed shot isn't on the other side of a wall
					FCollisionQueryParams Params(FName(TEXT("BulletWhip")), false, UTOwner);
					Params.AddIgnoredActor(PC->GetPawn());
					if (!GetWorld()->LineTraceTest(ClosestPt, ViewLoc, COLLISION_TRACE_WEAPON, Params))
					{
						PC->ClientHearSound(BulletWhip, this, ClosestPt, false, false, false);
					}
				}
			}
		}
	}
}

void AUTWeaponAttachment::FiringExtraUpdated()
{
}

void AUTWeaponAttachment::StopFiringEffects(bool bIgnoreCurrentMode)
{
	// we need to default to stopping all modes' firing effects as we can't rely on the replicated value to be correct at this point
	for (uint8 i = 0; i < MuzzleFlash.Num(); i++)
	{
		if (MuzzleFlash[i] != NULL && (!bIgnoreCurrentMode || !MuzzleFlash.IsValidIndex(UTOwner->FireMode) || MuzzleFlash[UTOwner->FireMode] == NULL || (i != UTOwner->FireMode && MuzzleFlash[i] != MuzzleFlash[UTOwner->FireMode])))
		{
			MuzzleFlash[i]->DeactivateSystem();
		}
	}
}
