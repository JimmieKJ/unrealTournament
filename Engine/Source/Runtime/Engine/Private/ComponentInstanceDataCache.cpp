// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ComponentInstanceDataCache.h"

FComponentInstanceDataBase::FComponentInstanceDataBase(const UActorComponent* SourceComponent)
{
	check(SourceComponent);
	SourceComponentName = SourceComponent->GetFName();
	SourceComponentTypeSerializedIndex = -1;
	
	AActor* ComponentOwner = SourceComponent->GetOwner();
	if (ComponentOwner)
	{
		bool bFound = false;
		for (const UActorComponent* SerializedComponent : ComponentOwner->SerializedComponents)
		{
			if (SerializedComponent)
			{
				if (SerializedComponent == SourceComponent)
				{
					++SourceComponentTypeSerializedIndex;
					bFound = true;
					break;
				}
				else if (SerializedComponent->GetClass() == SourceComponent->GetClass())
				{
					++SourceComponentTypeSerializedIndex;
				}
			}
		}
		if (!bFound)
		{
			SourceComponentTypeSerializedIndex = -1;
		}
	}
}

bool FComponentInstanceDataBase::MatchesComponent(const UActorComponent* Component) const
{
	bool bMatches = false;
	if (Component)
	{
		if (Component->GetFName() == SourceComponentName)
		{
			bMatches = true;
		}
		else if (SourceComponentTypeSerializedIndex >= 0)
		{
			int32 FoundSerializedComponentsOfType = -1;
			AActor* ComponentOwner = Component->GetOwner();
			if (ComponentOwner)
			{
				for (const UActorComponent* SerializedComponent : ComponentOwner->SerializedComponents)
				{
					if (   SerializedComponent
						&& (SerializedComponent->GetClass() == Component->GetClass())
						&& (++FoundSerializedComponentsOfType == SourceComponentTypeSerializedIndex))
					{
						bMatches = (SerializedComponent == Component);
						break;
					}
				}
			}
		}
	}
	return bMatches;
}

FComponentInstanceDataCache::FComponentInstanceDataCache(const AActor* Actor)
{
	if(Actor != NULL)
	{
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);

		// Grab per-instance data we want to persist
		for (UActorComponent* Component : Components)
		{
			if(Component->bCreatedByConstructionScript) // Only cache data from 'created by construction script' components
			{
				FComponentInstanceDataBase* ComponentInstanceData = Component->GetComponentInstanceData();
				if (ComponentInstanceData)
				{
					check(!Component->GetComponentInstanceDataType().IsNone());
					TypeToDataMap.Add(Component->GetComponentInstanceDataType(), ComponentInstanceData);
				}
			}
		}
	}
}

FComponentInstanceDataCache::~FComponentInstanceDataCache()
{
	for (auto InstanceDataPair : TypeToDataMap)
	{
		delete InstanceDataPair.Value;
	}
}

void FComponentInstanceDataCache::ApplyToActor(AActor* Actor) const
{
	if(Actor != NULL)
	{
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);

		// Apply per-instance data.
		for (UActorComponent* Component : Components)
		{
			if(Component->bCreatedByConstructionScript) // Only try and apply data to 'created by construction script' components
			{
				const FName ComponentInstanceDataType = Component->GetComponentInstanceDataType();

				if (!ComponentInstanceDataType.IsNone())
				{
					TArray< FComponentInstanceDataBase* > CachedData;
					TypeToDataMap.MultiFind(ComponentInstanceDataType, CachedData);

					for (FComponentInstanceDataBase* ComponentInstanceData : CachedData)
					{
						if (ComponentInstanceData && ComponentInstanceData->MatchesComponent(Component))
						{
							Component->ApplyComponentInstanceData(ComponentInstanceData);
							break;
						}
					}
				}
			}
		}
	}
}