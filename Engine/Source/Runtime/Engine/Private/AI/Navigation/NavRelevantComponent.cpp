// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/Navigation/NavRelevantComponent.h"

UNavRelevantComponent::UNavRelevantComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bNavigationRelevant = true;
	bAttachToOwnersRoot = true;
}

void UNavRelevantComponent::OnRegister()
{
	Super::OnRegister();

	CalcAndCacheBounds();

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
	CalcAndCacheBounds();
}

UObject* UNavRelevantComponent::GetNavigationParent() const
{
	if (bAttachToOwnersRoot)
	{
		AActor* OwnerActor = GetOwner();
		return OwnerActor && OwnerActor->GetRootComponent() ? static_cast<UObject*>(OwnerActor->GetRootComponent()) : static_cast<UObject*>(OwnerActor);
	}

	return nullptr;
}

void UNavRelevantComponent::CalcBounds()
{
	CalcAndCacheBounds();
}

void UNavRelevantComponent::CalcAndCacheBounds() const
{
	const AActor* OwnerActor = GetOwner();
	const FVector MyLocation = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;

	Bounds = FBox::BuildAABB(MyLocation, FVector(100.0f, 100.0f, 100.0f));
}

void UNavRelevantComponent::ForceNavigationRelevancy(bool bForce)
{
	bAttachToOwnersRoot = !bForce;
	if (bForce)
	{
		bNavigationRelevant = true;
	}

	RefreshNavigationModifiers();
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
