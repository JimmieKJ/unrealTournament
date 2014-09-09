// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTLift.h"
#include "UTCharacterMovement.h"

AUTLift::AUTLift(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	LastEncroachNotifyTime = -10.f;
	bAlwaysRelevant = true;
	LiftVelocity = FVector(0.f);
	TickLocation = EncroachComponent ? EncroachComponent->GetComponentLocation() : GetActorLocation();
}

void AUTLift::SetEncroachComponent(class UPrimitiveComponent* NewEncroachComponent)
{
	if (EncroachComponent && EncroachComponent->OnComponentBeginOverlap.IsBound())
	{
		EncroachComponent->OnComponentBeginOverlap.RemoveDynamic(this, &AUTLift::OnOverlapBegin);
	} 
	EncroachComponent = NewEncroachComponent; 
	if (EncroachComponent)
	{
		EncroachComponent->bGenerateOverlapEvents = true;
		EncroachComponent->OnComponentBeginOverlap.AddDynamic(this, &AUTLift::OnOverlapBegin);
	}
}

void AUTLift::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		//UE_LOG(UT, Warning, TEXT("Overlapping %s relative position %f"), *OtherActor->GetName(), GetActorLocation().Z - OtherActor->GetActorLocation().Z);
		OnEncroachActor(OtherActor);
	}
}

void AUTLift::ReceiveHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bSelfMoved && EncroachComponent)
	{
		//if (GetWorld()->GetTimeSeconds() - LastEncroachNotifyTime > 0.2f)
		{
			if (Cast<AUTProjectile>(Other))
			{
				if (bMoveWasBlocked)
				{
					// projectile didn't get out of the way
					Cast<AUTProjectile>(Other)->Explode(HitLocation, HitNormal, MyComp);
				}
				bMoveWasBlocked = true;
				return;
			}
			//if (Other) { UE_LOG(UT, Warning, TEXT("RECEIVE HIT %s"), *Other->GetName()); }
			AUTCharacter* UTChar = Cast<AUTCharacter>(Other);
			if (UTChar && UTChar->IsDead())
			{
				UTChar->Destroy();
				return;
			}
			if (UTChar && UTChar->UTCharacterMovement && UTChar->UTCharacterMovement->CanBaseOnLift(EncroachComponent, LiftEndLocation - EncroachComponent->GetComponentLocation()))
			{
				// if UTCharacter could stand on me, then base him and keep going
				bMoveWasBlocked = true;
				return;
			}
			if (Cast<AUTGib>(Other))
			{
				Other->Destroy();
				return;
			}
			OnEncroachActor(Other);
			LastEncroachNotifyTime = GetWorld()->GetTimeSeconds();
		}
	}
}

// @TODO FIXMESTEVE support lifts on other lifts, relative movement, and rotation
void AUTLift::MoveLiftTo(FVector NewLocation, FRotator NewRotation)
{
	if (EncroachComponent && (!NewLocation.Equals(EncroachComponent->GetComponentLocation()) || !NewRotation.Equals(EncroachComponent->GetComponentRotation())))
	{
		bMoveWasBlocked = false;
		LiftStartLocation = EncroachComponent->GetComponentLocation();
		LiftEndLocation = NewLocation;
		EncroachComponent->MoveComponent(NewLocation - EncroachComponent->GetComponentLocation(), EncroachComponent->GetComponentRotation(), true, NULL, MOVECOMP_IgnoreBases);

		// @TODO FIXMESTEVE - really correct would be recurse (with cap)
		if (bMoveWasBlocked)
		{
			LiftStartLocation = EncroachComponent->GetComponentLocation();
			EncroachComponent->MoveComponent(NewLocation - EncroachComponent->GetComponentLocation(), EncroachComponent->GetComponentRotation(), true, NULL, MOVECOMP_IgnoreBases);
		}
	}
}

void AUTLift::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (EncroachComponent && (DeltaTime > 0.f))
	{
		FVector NewLoc = EncroachComponent->GetComponentLocation();
		LiftVelocity = (NewLoc - TickLocation) / DeltaTime;
		EncroachComponent->ComponentVelocity = LiftVelocity;
		TickLocation = NewLoc;
	}
}

FVector AUTLift::GetVelocity() const
{
	return LiftVelocity;
}
