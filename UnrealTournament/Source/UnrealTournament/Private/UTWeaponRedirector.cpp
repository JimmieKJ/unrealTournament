// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeaponRedirector.h"
#include "UnrealNetwork.h"
#include "UTWorldSettings.h"
#include "UTProj_TransDisk.h"

void AUTWeaponRedirector::InitFor_Implementation(APawn* InInstigator, const FRepCollisionShape& InCollision, UPrimitiveComponent* InBase, const FTransform& InDest)
{
	Instigator = InInstigator;
	PortalDest = InDest;
	PortalDest.RemoveScaling();
	CollisionInfo = InCollision;
	UpdateCollisionShape();
	RootComponent->AttachTo(InBase, NAME_None, EAttachLocation::KeepWorldPosition);
	bReplicateMovement = (InBase != NULL);
}

void AUTWeaponRedirector::UpdateCollisionShape()
{
	if (CollisionComp != NULL)
	{
		CollisionComp->DetachFromParent();
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
		CollisionComp->AttachTo(RootComponent, NAME_None, EAttachLocation::SnapToTarget);
		CollisionComp->RegisterComponent();
		CollisionComp->UpdateOverlaps();
	}
}

float AUTWeaponRedirector::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// note: we don't have any good way to prevent Instigator from getting hit again from here if it's an AoE that might also hit it directly (e.g. very short teleport)
	return (Instigator != NULL && bRedirectDamage) ? Instigator->TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser) : 0.0f;
}

void AUTWeaponRedirector::OnBeginOverlap(AActor* OtherActor)
{
	if (bWeaponPortal && (Cast<AUTProjectile>(OtherActor) != NULL) && (Cast<AUTProj_TransDisk>(OtherActor) == NULL))
	{
		FTransform OtherTransform = OtherActor->GetTransform();
		OtherTransform.RemoveScaling();
		FTransform StartTransform = OtherTransform;
		FTransform MyTransform = GetTransform();
		MyTransform.RemoveScaling();
		// convert to destination transform
		OtherTransform = OtherTransform.GetRelativeTransform(MyTransform) * PortalDest;
		if (OtherActor->TeleportTo(OtherTransform.GetTranslation(), OtherTransform.GetRotation().Rotator()))
		{
			if (GetWorld()->GetNetMode() != NM_DedicatedServer && WarpEffect != NULL)
			{
				AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
				if (WS == NULL || WS->EffectIsRelevant(OtherActor, StartTransform.GetTranslation(), true, false, 10000.0f, 500.0f, false))
				{
					UGameplayStatics::SpawnEmitterAtLocation(this, WarpEffect, StartTransform.GetLocation(), StartTransform.GetRotation().Rotator(), true);
				}
			}
		}
		// replicate movement for the projectile now, in case there are teleport discrepancies (timing differences are likely)
		OtherActor->bReplicateMovement = true;
	}
}

void AUTWeaponRedirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTWeaponRedirector, CollisionInfo);
	DOREPLIFETIME(AUTWeaponRedirector, PortalDest);
	DOREPLIFETIME(AUTWeaponRedirector, bWeaponPortal);
}