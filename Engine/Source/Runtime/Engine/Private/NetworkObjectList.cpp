// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Engine/NetworkObjectList.h"
#include "Engine/Level.h"
#include "EngineUtils.h"

void FNetworkObjectList::AddInitialObjects(UWorld* const World, const FName NetDriverName)
{
	if (World == nullptr)
	{
		return;
	}

	for (FActorIterator Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if (Actor != nullptr && !Actor->IsPendingKill() && ULevel::IsNetActor(Actor))
		{
			Add(Actor, NetDriverName);
		}
	}
}

void FNetworkObjectList::Add(AActor* const Actor, const FName NetDriverName)
{
	if (Actor == nullptr)
	{
		return;
	}

	if (!NetworkObjects.Contains(Actor))
	{
		// Special case the demo net driver, since actors currently only have one associated NetDriverName.
		if (Actor->GetNetDriverName() == NetDriverName || NetDriverName == NAME_DemoNetDriver)
		{
			NetworkObjects.Emplace(new FNetworkObjectInfo(Actor));
		}
	}
}
