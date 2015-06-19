// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/NavigationModifier.h"
#include "AI/Navigation/NavModifierComponent.h"
#include "AI/Navigation/NavAreas/NavArea_Null.h"

UNavModifierComponent::UNavModifierComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AreaClass = UNavArea_Null::StaticClass();
}

void UNavModifierComponent::CalcAndCacheBounds() const
{
	const AActor* MyOwner = GetOwner();
	if (MyOwner)
	{
		const float Radius = MyOwner->GetSimpleCollisionRadius();
		const float HalfHeght = MyOwner->GetSimpleCollisionHalfHeight();
		ObstacleExtent = FVector(Radius, Radius, HalfHeght);
		Bounds = FBox::BuildAABB(MyOwner->GetActorLocation(), ObstacleExtent);
	}
}

void UNavModifierComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	CalcAndCacheBounds();
	Data.Modifiers.Add(FAreaNavModifier(Bounds, FTransform::Identity, AreaClass));
}
