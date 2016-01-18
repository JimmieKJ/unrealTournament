// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/Tasks/AbilityTask_WaitOverlap.h"

UAbilityTask_WaitOverlap::UAbilityTask_WaitOverlap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}


void UAbilityTask_WaitOverlap::OnOverlapCallback(AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && bFromSweep)
	{	
		//OnOverlap.Broadcast(SweepResult);
	}
}

void UAbilityTask_WaitOverlap::OnHitCallback(AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if(OtherActor)
	{
		// Construct TargetData
		FGameplayAbilityTargetData_SingleTargetHit * TargetData = new FGameplayAbilityTargetData_SingleTargetHit(Hit);

		// Give it a handle and return
		FGameplayAbilityTargetDataHandle	Handle;
		Handle.Data.Add(TSharedPtr<FGameplayAbilityTargetData>(TargetData));
		OnOverlap.Broadcast(Handle);

		// We are done. Kill us so we don't keep getting broadcast messages
		EndTask();
	}
}

/**
*	Need:
*	-Easy way to specify which primitive components should be used for this overlap test
*	-Easy way to specify which types of actors/collision overlaps that we care about/want to block on
*/

UAbilityTask_WaitOverlap* UAbilityTask_WaitOverlap::WaitForOverlap(UObject* WorldContextObject)
{
	auto MyObj = NewAbilityTask<UAbilityTask_WaitOverlap>(WorldContextObject);
	return MyObj;
}

void UAbilityTask_WaitOverlap::Activate()
{
	UPrimitiveComponent* PrimComponent = GetComponent();
	if (PrimComponent)
	{
		PrimComponent->OnComponentBeginOverlap.AddDynamic(this, &UAbilityTask_WaitOverlap::OnOverlapCallback);
		PrimComponent->OnComponentHit.AddDynamic(this, &UAbilityTask_WaitOverlap::OnHitCallback);
	}
}

void UAbilityTask_WaitOverlap::OnDestroy(bool AbilityEnded)
{
	UPrimitiveComponent* PrimComponent = GetComponent();
	if (PrimComponent)
	{
		PrimComponent->OnComponentBeginOverlap.RemoveDynamic(this, &UAbilityTask_WaitOverlap::OnOverlapCallback);
		PrimComponent->OnComponentHit.RemoveDynamic(this, &UAbilityTask_WaitOverlap::OnHitCallback);
	}

	Super::OnDestroy(AbilityEnded);
}

UPrimitiveComponent* UAbilityTask_WaitOverlap::GetComponent()
{
	// TEMP - we are just using root component's collision. A real system will need more data to specify which component to use
	UPrimitiveComponent * PrimComponent = nullptr;
	AActor* ActorOwner = GetAvatarActor();
	if (ActorOwner)
	{
		PrimComponent = Cast<UPrimitiveComponent>(ActorOwner->GetRootComponent());
		if (!PrimComponent)
		{
			PrimComponent = ActorOwner->FindComponentByClass<UPrimitiveComponent>();
		}
	}

	return PrimComponent;
}