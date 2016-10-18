// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_RocketGrenade.h"
#include "UnrealNetwork.h"

AUTProj_RocketGrenade::AUTProj_RocketGrenade(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bShouldBounce = true;

	MaxRotationRate = 90.0f;
	MaxBouncedRotationRate = 1080.0f;

	InitialLifeSpan = 0.0f;
	FuseTime = 1.5f;
	RandomFuseMod = 0.5f;

	MaxRandomBounce = 3.0f;

	RotationRate = FRotator(FMath::RandRange(-MaxRotationRate, MaxRotationRate), 
							FMath::RandRange(-MaxRotationRate, MaxRotationRate), 
							FMath::RandRange(-MaxRotationRate, MaxRotationRate));
}
void AUTProj_RocketGrenade::BeginPlay()
{
	if (Role == ROLE_Authority)
	{
		//Roll the dice
		RNGStream.GenerateNewSeed();
		Seed = RNGStream.GetCurrentSeed();

		//Set the fuse timer
		float ExplodeTime = FuseTime + FMath::FRand() * RandomFuseMod;
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_RocketGrenade::FuseTimer, FuseTime, true);
	}

	Super::BeginPlay();
}


void AUTProj_RocketGrenade::OnRep_Instigator()
{
	Super::OnRep_Instigator();
	if (Instigator != nullptr)
	{
		TArray<UParticleSystemComponent*> PSCs;
		GetComponents<UParticleSystemComponent>(PSCs);
		if (PSCs[0])
		{
			static FName NAME_TeamColor(TEXT("TeamColor"));
			AUTCharacter* UTChar = Cast<AUTCharacter>(Instigator);
			PSCs[0]->SetColorParameter(NAME_TeamColor, UTChar ? UTChar->GetTeamColor() : FVector(0.7f, 0.4f, 0.f));
		}
	}
}

void AUTProj_RocketGrenade::OnRep_Seed()
{
	RNGStream.Initialize(Seed);
}

void AUTProj_RocketGrenade::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//Spin the visual components
	if (GetNetMode() != NM_DedicatedServer)
	{
		const FQuat OldRot = GetActorQuat();
		const FQuat DeltaRot = (RotationRate * DeltaTime).Quaternion();
		SetActorRotation((DeltaRot * OldRot).Rotator());
	}
}

FRotator AUTProj_RocketGrenade::GetRandomRotator(float MinMax)
{
	FVector v = RNGStream.GetUnitVector();
	Seed = RNGStream.GetCurrentSeed();
	return FRotator(v.X * MinMax, v.Y  * MinMax, v.Z * MinMax);
}

void AUTProj_RocketGrenade::OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	bCanHitInstigator = true;

	//Spin faster than the initial launch
	if (GetNetMode() != NM_DedicatedServer)
	{
		RotationRate = FRotator(FMath::RandRange(-MaxBouncedRotationRate, MaxBouncedRotationRate), 
								FMath::RandRange(-MaxBouncedRotationRate, MaxBouncedRotationRate), 
								FMath::RandRange(-MaxBouncedRotationRate, MaxBouncedRotationRate));
	}

	//Random bounce direction
	FRotator RandRot = GetRandomRotator(MaxRandomBounce);
	ProjectileMovement->Velocity = RandRot.RotateVector(ProjectileMovement->Velocity);
	ProjectileMovement->Bounciness = 0.3f;
	Super::OnBounce(ImpactResult, ImpactVelocity);
}

void AUTProj_RocketGrenade::FuseTimer()
{
	Explode(GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
}

void AUTProj_RocketGrenade::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTProj_RocketGrenade, Seed, COND_InitialOnly);
}