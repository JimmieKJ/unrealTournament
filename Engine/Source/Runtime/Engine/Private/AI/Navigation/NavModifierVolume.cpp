// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/NavigationModifier.h"
#include "AI/Navigation/NavModifierVolume.h"
#include "AI/Navigation/NavAreas/NavArea_Null.h"
#include "AI/NavigationOctree.h"

//----------------------------------------------------------------------//
// ANavModifierVolume
//----------------------------------------------------------------------//
ANavModifierVolume::ANavModifierVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AreaClass(UNavArea_Null::StaticClass())
{
}

void ANavModifierVolume::GetNavigationData(FNavigationRelevantData& Data) const
{
	if (Brush && AreaClass && AreaClass != UNavigationSystem::GetDefaultWalkableArea())
	{
		FAreaNavModifier AreaMod(GetBrushComponent(), AreaClass);
		Data.Modifiers.Add(AreaMod);
	}
}

FBox ANavModifierVolume::GetNavigationBounds() const
{
	return GetComponentsBoundingBox();
}

void ANavModifierVolume::SetAreaClass(TSubclassOf<UNavArea> NewAreaClass)
{
	if (NewAreaClass != AreaClass)
	{
		AreaClass = NewAreaClass;

		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
		if (NavSys != nullptr)
		{
			NavSys->UpdateActorInNavOctree(*this);
		}
	}
}

#if WITH_EDITOR

void ANavModifierVolume::PostEditUndo()
{
	Super::PostEditUndo();

	UNavigationSystem::UpdateActorInNavOctree(*this);
}

void ANavModifierVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ANavModifierVolume, AreaClass))
	{
		UNavigationSystem::UpdateActorInNavOctree(*this);
	}
}

#endif
