// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGib.h"
#include "UTWorldSettings.h"

AUTGib::AUTGib(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Mesh = ObjectInitializer.CreateOptionalDefaultSubobject<UStaticMeshComponent>(this, FName(TEXT("Mesh")));
	if (Mesh != NULL)
	{
		Mesh->bReceivesDecals = false;
		Mesh->SetCollisionProfileName(FName(TEXT("CharacterMesh")));
		Mesh->SetCollisionObjectType(ECC_PhysicsBody);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetNotifyRigidBodyCollision(true);
		Mesh->OnComponentHit.AddDynamic(this, &AUTGib::OnPhysicsCollision);
	}
	RootComponent = Mesh;

	InvisibleLifeSpan = 7.f;
	InitialLifeSpan = 10.0f;
}

void AUTGib::PreInitializeComponents()
{
	LastBloodTime = GetWorld()->TimeSeconds;
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

void AUTGib::BeginPlay()
{
	Super::BeginPlay();

	if (!IsPendingKillPending())
	{
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGib::CheckGibVisibility, 7.f, false);
	}
}

void AUTGib::CheckGibVisibility()
{
	if (GetWorld()->GetTimeSeconds() - GetLastRenderTime() > 1.f)
	{
		Destroy();
	}
}

void AUTGib::OnPhysicsCollision(AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
#if !UE_SERVER
	// if we landed on a mover, attach to it
	if (OtherComp != NULL && OtherComp->Mobility == EComponentMobility::Movable && (Hit.Normal.Z > 0.7f))
	{
		RootComponent->AttachTo(OtherComp, NAME_None, EAttachLocation::KeepWorldPosition);
	}

	// maybe spawn blood as we smack into things
	if (OtherComp != NULL && Cast<AUTGib>(OtherActor) == NULL && GetWorld()->TimeSeconds - LastBloodTime > 0.5f && GetWorld()->TimeSeconds - GetLastRenderTime() < 0.5f)
	{
		if (BloodEffects.Num() > 0)
		{
			UParticleSystem* Effect = BloodEffects[FMath::RandHelper(BloodEffects.Num())];
			if (Effect != NULL)
			{
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Effect, Hit.Location, Hit.Normal.Rotation(), true);
			}
		}
		AUTWorldSettings* Settings = Cast<AUTWorldSettings>(GetWorldSettings());
		if (Settings != NULL)
		{
			if (BloodDecals.Num() > 0)
			{
				const FBloodDecalInfo& DecalInfo = BloodDecals[FMath::RandHelper(BloodDecals.Num())];
				if (DecalInfo.Material != NULL)
				{
					static FName NAME_BloodDecal(TEXT("BloodDecal"));
					FHitResult DecalHit;
					if (GetWorld()->LineTraceSingleByChannel(DecalHit, GetActorLocation(), GetActorLocation() - Hit.Normal * 200.0f, ECC_Visibility, FCollisionQueryParams(NAME_BloodDecal, false, this)) && Hit.Component->bReceivesDecals)
					{
						UDecalComponent* Decal = NewObject<UDecalComponent>(GetWorld());
						if (Hit.Component.Get() != NULL && Hit.Component->Mobility == EComponentMobility::Movable)
						{
							Decal->SetAbsolute(false, false, true);
							Decal->AttachTo(Hit.Component.Get());
						}
						else
						{
							Decal->SetAbsolute(true, true, true);
						}
						FVector2D DecalScale = DecalInfo.BaseScale * FMath::FRandRange(DecalInfo.ScaleMultRange.X, DecalInfo.ScaleMultRange.Y);
						Decal->SetWorldScale3D(FVector(1.0f, DecalScale.X, DecalScale.Y));
						Decal->SetWorldLocation(DecalHit.Location);
						Decal->SetWorldRotation((-DecalHit.Normal).Rotation() + FRotator(0.0f, 0.0f, 360.0f * FMath::FRand()));
						Decal->SetDecalMaterial(DecalInfo.Material);
						Decal->RegisterComponentWithWorld(GetWorld());
						Settings->AddImpactEffect(Decal);
					}
				}
			}
		}
	
		LastBloodTime = GetWorld()->TimeSeconds;
	}
#endif
}