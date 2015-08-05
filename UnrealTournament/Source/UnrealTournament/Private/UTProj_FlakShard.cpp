// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectileMovementComponent.h"
#include "UTProj_FlakShard.h"
#include "Particles/ParticleSystemComponent.h"

AUTProj_FlakShard::AUTProj_FlakShard(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mesh = ObjectInitializer.CreateOptionalDefaultSubobject<UStaticMeshComponent>(this, FName(TEXT("Mesh")));
	if (Mesh != NULL)
	{
		Mesh->SetCollisionProfileName(FName(TEXT("NoCollision")));
		Mesh->AttachParent = RootComponent;
		Mesh->bReceivesDecals = false;
		Mesh->bUseAsOccluder = false;
	}

	Trail = ObjectInitializer.CreateOptionalDefaultSubobject<UParticleSystemComponent>(this, FName(TEXT("Trail")));
	if (Trail != NULL)
	{
		Trail->AttachParent = RootComponent;
	}

	HeatFadeTime = 1.0f;
	HotTrailColor = FLinearColor(2.0f, 1.65f, 0.65f, 1.0f);
	ColdTrailColor = FLinearColor(0.165f, 0.135f, 0.097f, 0.0f);

	ProjectileMovement->InitialSpeed = 7600.0f;
	ProjectileMovement->MaxSpeed = 7600.0f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 0.0f;

	// Damage
	DamageParams.BaseDamage = 18.0f;
	DamageParams.MinimumDamage = 5.0f;
	Momentum = 20000.f;

	DamageAttenuation = 15.0f;
	DamageAttenuationDelay = 0.5f;
	MinDamageSpeed = 800.f;

	SelfDamageAttenuation = 25.0f;
	SelfDamageAttenuationDelay = 0.11f;

	InitialLifeSpan = 2.f;
	BouncesRemaining = 2;
	FirstBounceDamping = 0.9f;
	BounceDamping = 0.75f;
	BounceDamagePct = 0.8f;
	RandomBounceCone = 0.2f;
	FullGravityDelay = 0.5f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bNetTemporary = true;
	NumSatelliteShards = 1;
	StatsHitCredit = 0.111f;
}

void AUTProj_FlakShard::BeginPlay()
{
	Super::BeginPlay();

	if (!IsPendingKillPending() && (GetNetMode() != NM_DedicatedServer) && !DisableEmitterLights())
	{
		UStaticMeshComponent* ShardMesh = Cast<UStaticMeshComponent>(Mesh);
		if (ShardMesh)
		{
			for (int32 i = 0; i < NumSatelliteShards; i++)
			{
				FVector ShardOffset = OverlapRadius * FVector(0.f, FMath::FRand() - 0.5f, FMath::FRand() - 0.5f).GetSafeNormal();
				UStaticMeshComponent* NewMesh = NewObject<UStaticMeshComponent>(this);
				NewMesh->SetStaticMesh(ShardMesh->StaticMesh);
				NewMesh->SetMaterial(0, ShardMesh->GetMaterial(0));
				NewMesh->RegisterComponentWithWorld(GetWorld());
				NewMesh->AttachTo(CollisionComp);
				NewMesh->SetRelativeLocation(ShardOffset);
				NewMesh->SetRelativeScale3D(ShardMesh->RelativeScale3D);
				NewMesh->bGenerateOverlapEvents = false;
				SatelliteShards.Add(NewMesh);
			}
		}
	}
}

void AUTProj_FlakShard::CatchupTick(float CatchupTickDelta)
{
	Super::CatchupTick(CatchupTickDelta);
	FullGravityDelay -= CatchupTickDelta;
	// @TODO FIXMESTEVE - not synchronizing this correctly on other clients unless we replicate it.  Needed?
}

void AUTProj_FlakShard::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (Mesh != NULL)
	{
		Mesh->SetRelativeRotation(FRotator(360.0f * FMath::FRand(), 360.0f * FMath::FRand(), 360.0f * FMath::FRand()));
		if (MeshMI == NULL)
		{
			MeshMI = Mesh->CreateAndSetMaterialInstanceDynamic(0);
		}
	}
}

void AUTProj_FlakShard::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	if (GetVelocity().Size() < MinDamageSpeed)
	{
		ShutDown();
	}
	else
	{
		Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
	}
}

void AUTProj_FlakShard::OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	RemoveSatelliteShards();
	if (GetWorld()->GetTimeSeconds() - CreationTime > 2.f * FullGravityDelay)
	{
		ShutDown();
		return;
	}
	float CurrentBounceDamping = (ProjectileMovement->ProjectileGravityScale == 0.f) ? FirstBounceDamping : BounceDamping;
	Super::OnBounce(ImpactResult, ImpactVelocity);

	// manually handle bounce velocity to match UT3 for now
	ProjectileMovement->Velocity = CurrentBounceDamping * (ImpactVelocity - 2.0f * ImpactResult.Normal * (ImpactVelocity | ImpactResult.Normal));

	// add random bounce factor
	ProjectileMovement->Velocity = ProjectileMovement->Velocity.Size() * FMath::VRandCone(ProjectileMovement->Velocity, RandomBounceCone);

	// Set gravity on bounce
	ProjectileMovement->ProjectileGravityScale = 1.f;

	// Limit number of bounces
	BouncesRemaining--;
	if (BouncesRemaining == 0)
	{
		ProjectileMovement->bShouldBounce = false;
	}
}

void AUTProj_FlakShard::RemoveSatelliteShards()
{
	// remove satellite shards
	for (int32 i = 0; i < SatelliteShards.Num(); i++)
	{
		if (SatelliteShards[i])
		{
			SatelliteShards[i]->DetachFromParent();
			SatelliteShards[i]->SetHiddenInGame(true);
		}
	}
	SatelliteShards.Empty();
}

FRadialDamageParams AUTProj_FlakShard::GetDamageParams_Implementation(AActor* OtherActor, const FVector& HitLocation, float& OutMomentum) const
{
	FRadialDamageParams Result = Super::GetDamageParams_Implementation(OtherActor, HitLocation, OutMomentum);
	if (OtherActor == Instigator)
	{
		// attenuate self damage and momentum
		Result.BaseDamage = FMath::Max<float>(0.f, Result.BaseDamage - SelfDamageAttenuation * FMath::Max<float>(0.0f, GetWorld()->GetTimeSeconds() - CreationTime - SelfDamageAttenuationDelay));
	}
	else
	{
		Result.BaseDamage = FMath::Max<float>(Result.MinimumDamage, Result.BaseDamage - DamageAttenuation * FMath::Max<float>(0.0f, GetWorld()->GetTimeSeconds() - CreationTime - DamageAttenuationDelay));
		if (ProjectileMovement->ProjectileGravityScale != 0.f)
		{
			Result.BaseDamage *= BounceDamagePct;
		}
	}
	return Result;
}

void AUTProj_FlakShard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (InitialLifeSpan - GetLifeSpan() > FullGravityDelay)
	{
		ProjectileMovement->ProjectileGravityScale = 1.f;
		RemoveSatelliteShards();
	}

	if (HeatFadeTime > 0.0f)
	{
		float FadePct = FMath::Min<float>(1.0f, (GetWorld()->TimeSeconds - CreationTime) / HeatFadeTime);
		if (Trail != NULL)
		{
			FLinearColor TrailColor = FMath::Lerp<FLinearColor>(HotTrailColor, ColdTrailColor, FadePct);
			Trail->SetColorParameter(NAME_Color, TrailColor);
			static FName NAME_StartAlpha(TEXT("StartAlpha"));
			Trail->SetFloatParameter(NAME_StartAlpha, TrailColor.A);
		}
		if (MeshMI != NULL)
		{
			static FName NAME_FadePct(TEXT("FadePct"));
			MeshMI->SetScalarParameterValue(NAME_FadePct, FadePct);
		}
	}
}
