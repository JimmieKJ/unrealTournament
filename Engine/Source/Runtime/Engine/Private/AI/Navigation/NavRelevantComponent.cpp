// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/Navigation/NavRelevantComponent.h"

UNavRelevantComponent::UNavRelevantComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bNavigationRelevant = true;
}

void UNavRelevantComponent::OnRegister()
{
	Super::OnRegister();

	CalcBounds();

	UNavigationSystem::OnComponentRegistered(this);
}

void UNavRelevantComponent::OnUnregister()
{
	Super::OnUnregister();

	UNavigationSystem::OnComponentUnregistered(this);
}

FBox UNavRelevantComponent::GetNavigationBounds() const
{
	return Bounds;
}

bool UNavRelevantComponent::IsNavigationRelevant() const
{
	return bNavigationRelevant;
}

void UNavRelevantComponent::UpdateNavigationBounds()
{
	CalcBounds();
}

void UNavRelevantComponent::CalcBounds()
{
	AActor* OwnerActor = GetOwner();
	const FVector MyLocation = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;

	Bounds = FBox::BuildAABB(MyLocation, FVector(100.0f, 100.0f, 100.0f));
}

void UNavRelevantComponent::SetNavigationRelevancy(bool bRelevant)
{
	if (bNavigationRelevant != bRelevant)
	{
		bNavigationRelevant = bRelevant;
		RefreshNavigationModifiers();
	}
}

void UNavRelevantComponent::RefreshNavigationModifiers()
{
	UNavigationSystem::UpdateNavOctree(this);
}
