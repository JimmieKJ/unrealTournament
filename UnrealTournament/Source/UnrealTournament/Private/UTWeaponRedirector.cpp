// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeaponRedirector.h"
#include "UnrealNetwork.h"
#include "UTWorldSettings.h"
#include "UTProj_TransDisk.h"
#include "UTReplicatedEmitter.h"

void AUTWeaponRedirector::InitFor_Implementation(APawn* InInstigator, const FRepCollisionShape& InCollision, UPrimitiveComponent* InBase, const FTransform& InDest)
{
	Instigator = InInstigator;
	PortalDest = InDest;
	PortalDest.RemoveScaling();
	CollisionInfo = InCollision;
	UpdateCollisionShape(); 
	if (InBase != nullptr)
	{
		RootComponent->AttachToComponent(InBase, FAttachmentTransformRules::KeepWorldTransform);
	}
	bReplicateMovement = (InBase != NULL);
}

void AUTWeaponRedirector::UpdateCollisionShape()
{
	if (CollisionComp != NULL)
	{
		CollisionComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		CollisionComp->DestroyComponent();
		CollisionComp = NULL;
	}

	switch (CollisionInfo.ShapeType)
	{
		case ECollisionShape::Box:
			CollisionComp = NewObject<UBoxComponent>(this);
			((UBoxComponent*)CollisionComp)->SetBoxExtent(CollisionInfo.GetExtent(), false);
			break;
		case ECollisionShape::Sphere:
			CollisionComp = NewObject<USphereComponent>(this);
			((USphereComponent*)CollisionComp)->SetSphereRadius(CollisionInfo.GetSphereRadius(), false);
			break;
		case ECollisionShape::Capsule:
			CollisionComp = NewObject<UCapsuleComponent>(this);
			((UCapsuleComponent*)CollisionComp)->SetCapsuleSize(CollisionInfo.GetCapsuleRadius(), CollisionInfo.GetCapsuleHalfHeight(), false);
			break;
	}

	if (CollisionComp != NULL)
	{
		CollisionComp->SetCollisionProfileName(FName(TEXT("OverlapAll")));
		CollisionComp->SetCollisionResponseToChannel(COLLISION_TRACE_WEAPON, ECR_Block);
		CollisionComp->SetCollisionResponseToChannel(COLLISION_TRACE_WEAPONNOCHARACTER, ECR_Block);
		CollisionComp->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
		CollisionComp->RegisterComponent();
		CollisionComp->UpdateOverlaps();
	}
}

float AUTWeaponRedirector::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// note: we don't have any good way to prevent Instigator from getting hit again from here if it's an AoE that might also hit it directly (e.g. very short teleport)
	if (Instigator != NULL && bRedirectDamage)
	{
		// @TODO flash main teleport effect as well client-side
		if (DamageEffect != NULL)
		{
			FHitResult HitInfo;
			{
				FVector UnusedDir;
				DamageEvent.GetBestHitInfo(this, DamageCauser, HitInfo, UnusedDir);
			}
			FVector const SpawnLocation = HitInfo.ImpactPoint;
			FRotator const SpawnRot = HitInfo.ImpactNormal.Rotation();
			FActorSpawnParameters Params;
			AActor* DE = GetWorld()->SpawnActor<AUTReplicatedEmitter>(DamageEffect, SpawnLocation, SpawnRot, Params);
		}
			
		return Instigator->TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	}

	return 0.f;
}

void AUTWeaponRedirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTWeaponRedirector, CollisionInfo);
}