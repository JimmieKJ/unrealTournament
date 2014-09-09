// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGib.h"
#include "UTWorldSettings.h"

AUTGib::AUTGib(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	Mesh = PCIP.CreateOptionalDefaultSubobject<UStaticMeshComponent>(this, FName(TEXT("Mesh")));
	if (Mesh != NULL)
	{
		Mesh->SetCollisionProfileName(FName(TEXT("CharacterMesh")));
		Mesh->SetCollisionObjectType(ECC_PhysicsBody);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetNotifyRigidBodyCollision(true);
		Mesh->OnComponentHit.AddDynamic(this, &AUTGib::OnPhysicsCollision);
	}
	RootComponent = Mesh;

	InvisibleLifeSpan = 10.0f;
	InitialLifeSpan = 15.0f;
}

void AUTGib::PreInitializeComponents()
{
	if (MeshChoices.Num() > 0 && Mesh != NULL)
	{
		if (Mesh->StaticMesh != NULL)
		{
			MeshChoices.AddUnique(Mesh->StaticMesh);
		}
		Mesh->SetStaticMesh(MeshChoices[FMath::RandHelper(MeshChoices.Num())]);
	}
	Super::PreInitializeComponents();
}

void AUTGib::OnPhysicsCollision(AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// maybe spawn blood as we smack into things
	if (OtherComp != NULL && OtherActor != this && GetWorld()->TimeSeconds - LastBloodTime > 0.5f && GetWorld()->TimeSeconds - GetLastRenderTime() < 1.0f)
	{
		if (BloodEffects.Num() > 0)
		{
			UParticleSystem* Effect = BloodEffects[FMath::RandHelper(BloodEffects.Num())];
			if (Effect != NULL)
			{
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Effect, Hit.Location, Hit.Normal.Rotation(), true);
			}
		}
		if (BloodDecals.Num() > 0)
		{
			const FBloodDecalInfo& DecalInfo = BloodDecals[FMath::RandHelper(BloodDecals.Num())];
			if (DecalInfo.Material != NULL)
			{
				// FIXME: hack to prevent decals from SM4- until engine is fixed (4.5?)
				if (GMaxRHIFeatureLevel <= ERHIFeatureLevel::SM4)
				{
					UMaterial* Mat = DecalInfo.Material->GetMaterial();
					if (Mat != NULL && Mat->DecalBlendMode > DBM_Emissive)
					{
						return;
					}
				}

				static FName NAME_BloodDecal(TEXT("BloodDecal"));
				FHitResult DecalHit;
				if (GetWorld()->LineTraceSingle(DecalHit, GetActorLocation(), GetActorLocation() - Hit.Normal * 200.0f, ECC_Visibility, FCollisionQueryParams(NAME_BloodDecal, false, this)))
				{
					UDecalComponent* Decal = ConstructObject<UDecalComponent>(UDecalComponent::StaticClass(), GetWorld());
					Decal->SetAbsolute(true, true, true);
					FVector2D DecalScale = DecalInfo.BaseScale * FMath::FRandRange(DecalInfo.ScaleMultRange.X, DecalInfo.ScaleMultRange.Y);
					Decal->SetWorldScale3D(FVector(1.0f, DecalScale.X, DecalScale.Y));
					Decal->SetWorldLocation(DecalHit.Location);
					Decal->SetWorldRotation((-DecalHit.Normal).Rotation() + FRotator(0.0f, 0.0f, 360.0f * FMath::FRand()));
					Decal->SetDecalMaterial(DecalInfo.Material);
					Decal->RegisterComponentWithWorld(GetWorld());
					AUTWorldSettings* Settings = Cast<AUTWorldSettings>(GetWorldSettings());
					if (Settings != NULL)
					{
						Settings->AddImpactEffect(Decal);
					}
					else
					{
						GetWorldTimerManager().SetTimer(Decal, &UDecalComponent::DestroyComponent, 30.0f, false);
					}
				}
			}
		}
		LastBloodTime = GetWorld()->TimeSeconds;
	}
}