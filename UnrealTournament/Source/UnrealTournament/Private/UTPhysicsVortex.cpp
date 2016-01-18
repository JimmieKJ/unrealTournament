// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPhysicsVortex.h"

AUTPhysicsVortex::AUTPhysicsVortex(const FObjectInitializer& OI)
: Super(OI)
{
	Radius = 900.0f;
	StartingForce = 400000.0f;
	ForcePerSecond = 400000.0f;
	InitialLifeSpan = 1.75f;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	RootComponent = OI.CreateDefaultSubobject<USceneComponent>(this, FName(TEXT("EmptyRoot")));
}

void AUTPhysicsVortex::Tick(float DeltaTime)
{
	float CurrentForce = StartingForce + ForcePerSecond * (GetWorld()->TimeSeconds - CreationTime);
	FVector MyLocation = GetActorLocation();

	TArray<FOverlapResult> Hits;
	static FName NAME_PhysicsVortex = FName(TEXT("PhysicsVortex"));
	FComponentQueryParams Params(NAME_PhysicsVortex, this);
	GetWorld()->OverlapMultiByObjectType(Hits, MyLocation, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);

	for (const FOverlapResult& TestHit : Hits)
	{
		if (TestHit.Actor.IsValid())
		{
			AUTCharacter* P = Cast<AUTCharacter>(TestHit.Actor.Get());
			if (P != NULL && P->IsRagdoll() && P->IsDead())
			{
				const FVector OtherLocation = P->GetActorLocation();
				if (!GetWorld()->LineTraceTestByChannel(MyLocation, OtherLocation, ECC_Visibility, Params, WorldResponseParams))
				{
					// if it has reached the center, gib it
					const FVector Dir = MyLocation - OtherLocation;
					if (Dir.Size() < P->GetMesh()->Bounds.SphereRadius && (P->GetVelocity().GetSafeNormal() | Dir) > 0.0/* TODO: && !class'GameInfo'.static.UseLowGore(WorldInfo)*/)
					{
						P->LastTakeHitInfo.DamageType = DamageType;
						P->GibExplosion();
					}
					else
					{
						P->GetMesh()->AddForce(Dir.GetSafeNormal() * CurrentForce);
					}
				}
			}
		}
	}
}