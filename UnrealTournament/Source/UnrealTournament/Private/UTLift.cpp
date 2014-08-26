// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTLift.h"

AUTLift::AUTLift(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	LastEncroachNotifyTime = -10.f;
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
	if (bSelfMoved)
	{
		if (GetWorld()->GetTimeSeconds() - LastEncroachNotifyTime > 0.2f)
		{
			//if (Other) { UE_LOG(UT, Warning, TEXT("RECEIVE HIT %s"), *Other->GetName()); }
			OnEncroachActor(Other);
			LastEncroachNotifyTime = GetWorld()->GetTimeSeconds();
		}
	}
}

// @TODO FIXMESTEVE support lifts on other lifts, relative movement, and rotation
// @TODO FIXMESTEVE no triggers
void AUTLift::MoveLiftTo(FVector NewLocation, FRotator NewRotation)
{
	if (EncroachComponent && (!NewLocation.Equals(EncroachComponent->GetComponentLocation()) || !NewRotation.Equals(EncroachComponent->GetComponentRotation())))
	{
		EncroachComponent->MoveComponent(NewLocation - EncroachComponent->GetComponentLocation(), EncroachComponent->GetComponentRotation(), true, NULL, MOVECOMP_IgnoreBases);
	}
}
