// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealTournament.h"
#include "UTImpactEffect.h"
#include "UTWeaponStateFiringBurst.h"
#include "UTWeapAttachment_Enforcer.h"


AUTWeapAttachment_Enforcer::AUTWeapAttachment_Enforcer(const FObjectInitializer& OI)
: Super(OI)
{
	LeftMesh = OI.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("Mesh3P_Left"));

	LeftMesh->AttachParent = RootComponent;
	LeftMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	LeftAttachSocket = FName((TEXT("LeftWeaponPoint")));

	BurstSize = 3;
	AlternateCount = 0;
}


void AUTWeapAttachment_Enforcer::BeginPlay()
{
	Super::BeginPlay();

	UTOwner = Cast<AUTCharacter>(Instigator);
	if (UTOwner != NULL)
	{
		AttachToOwner();
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("UTWeaponAttachment: Bad Instigator: %s"), *GetNameSafe(Instigator));
	}
}

void AUTWeapAttachment_Enforcer::AttachToOwnerNative()
{
	LeftMesh->AttachTo(UTOwner->GetMesh(), LeftAttachSocket);

	LeftMesh->SetRelativeLocation(LeftAttachOffset);
	LeftMesh->bRecentlyRendered = UTOwner->GetMesh()->bRecentlyRendered;
	LeftMesh->LastRenderTime = UTOwner->GetMesh()->LastRenderTime;
	UpdateOverlays();
	SetSkin(UTOwner->GetSkin());

	Super::AttachToOwnerNative();
}

void AUTWeapAttachment_Enforcer::UpdateOverlays()
{
	WeaponType.GetDefaultObject()->UpdateOverlaysShared(this, UTOwner, Mesh, OverlayMesh);
	WeaponType.GetDefaultObject()->UpdateOverlaysShared(this, UTOwner, LeftMesh, LeftOverlayMesh);
}

void AUTWeapAttachment_Enforcer::SetSkin(UMaterialInterface* NewSkin)
{
	Super::SetSkin(NewSkin);
	if (NewSkin != NULL)
	{
		for (int32 i = 0; i < LeftMesh->GetNumMaterials(); i++)
		{
			LeftMesh->SetMaterial(i, NewSkin);
		}
	}
	else
	{
		for (int32 i = 0; i < LeftMesh->GetNumMaterials(); i++)
		{
			LeftMesh->SetMaterial(i, GetClass()->GetDefaultObject<AUTWeapAttachment_Enforcer>()->LeftMesh->GetMaterial(i));
		}
	}
}

void AUTWeapAttachment_Enforcer::PlayFiringEffects()
{
	//Find out if we're in burst fire mode
	if (GetNetMode() != NM_DedicatedServer)
	{
		uint8 CalculatedMuzzleFlashIndex = UTOwner->FireMode;

		//MuzzleFlash indices 3 & 4 are for left weapon
			
		//The only way in which the current weapon state is replicated is through the current firemode
		//Assume that it's configured so that 1 == WeaponStateFiring_Enforcer and 2 == WeaponStateFiringBurst_Enforcer
		if ((UTOwner->FireMode == 1 && AlternateCount / BurstSize > 0) || (UTOwner->FireMode == 0 && AlternateCount % 2 == 1))
		{
				CalculatedMuzzleFlashIndex += 2;
		}

		// stop any firing effects for other firemodes
		// this is needed because if the user swaps firemodes very quickly replication might not receive a discrete stop and start new
		Super::StopFiringEffects(true);

		// muzzle flash
		if (MuzzleFlash.IsValidIndex(CalculatedMuzzleFlashIndex) && MuzzleFlash[CalculatedMuzzleFlashIndex] != NULL && MuzzleFlash[CalculatedMuzzleFlashIndex]->Template != NULL)
		{
			// if we detect a looping particle system, then don't reactivate it
			if (!MuzzleFlash[CalculatedMuzzleFlashIndex]->bIsActive || MuzzleFlash[CalculatedMuzzleFlashIndex]->bSuppressSpawning || !IsLoopingParticleSystem(MuzzleFlash[CalculatedMuzzleFlashIndex]->Template))
			{
				MuzzleFlash[CalculatedMuzzleFlashIndex]->ActivateSystem();
			}
		}

		// fire effects
		static FName NAME_HitLocation(TEXT("HitLocation"));
		static FName NAME_LocalHitLocation(TEXT("LocalHitLocation"));

		const FVector SpawnLocation = (MuzzleFlash.IsValidIndex(CalculatedMuzzleFlashIndex) && MuzzleFlash[CalculatedMuzzleFlashIndex] != NULL) ? MuzzleFlash[CalculatedMuzzleFlashIndex]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetActorRotation().RotateVector(FVector(UTOwner->GetSimpleCollisionCylinderExtent().X, 0.0f, 0.0f));

		if (FireEffect.IsValidIndex(CalculatedMuzzleFlashIndex) && FireEffect[CalculatedMuzzleFlashIndex] != NULL)
		{
			UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FireEffect[CalculatedMuzzleFlashIndex], SpawnLocation, (UTOwner->FlashLocation - SpawnLocation).Rotation(), true);
			PSC->SetVectorParameter(NAME_HitLocation, UTOwner->FlashLocation);
			PSC->SetVectorParameter(NAME_LocalHitLocation, PSC->ComponentToWorld.InverseTransformPosition(UTOwner->FlashLocation));
		}

		// perhaps the muzzle flash also contains hit effect (constant beam, etc) so set the parameter on it instead
		else if (MuzzleFlash.IsValidIndex(CalculatedMuzzleFlashIndex) && MuzzleFlash[CalculatedMuzzleFlashIndex] != NULL)
		{
			MuzzleFlash[CalculatedMuzzleFlashIndex]->SetVectorParameter(NAME_HitLocation, UTOwner->FlashLocation);
			MuzzleFlash[CalculatedMuzzleFlashIndex]->SetVectorParameter(NAME_LocalHitLocation, MuzzleFlash[CalculatedMuzzleFlashIndex]->ComponentToWorld.InverseTransformPosition(UTOwner->FlashLocation));
		}

		if ((UTOwner->FlashLocation - LastImpactEffectLocation).Size() >= ImpactEffectSkipDistance || GetWorld()->TimeSeconds - LastImpactEffectTime >= MaxImpactEffectSkipTime)
		{
			if (ImpactEffect.IsValidIndex(UTOwner->FireMode) && ImpactEffect[UTOwner->FireMode] != NULL)
			{
				FHitResult ImpactHit = AUTWeapon::GetImpactEffectHit(UTOwner, SpawnLocation, UTOwner->FlashLocation);
				if (!CancelImpactEffect(ImpactHit))
				{
					ImpactEffect[UTOwner->FireMode].GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(ImpactHit.Normal.Rotation(), ImpactHit.Location), ImpactHit.Component.Get(), NULL, UTOwner->Controller);
				}
			}
			LastImpactEffectLocation = UTOwner->FlashLocation;
			LastImpactEffectTime = GetWorld()->TimeSeconds;
		}

		PlayBulletWhip();

		AlternateCount++;

		if (UTOwner->FireMode == 1 && AlternateCount == BurstSize * 2)
		{
			AlternateCount = 0;
		}
	}
}

void AUTWeapAttachment_Enforcer::StopFiringEffects(bool bIgnoreCurrentMode)
{
	AlternateCount = 0;
	Super::StopFiringEffects(bIgnoreCurrentMode);
}

