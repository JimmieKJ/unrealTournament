// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ComponentInstanceDataCache.h"

FComponentInstanceDataBase::FComponentInstanceDataBase(const UActorComponent* SourceComponent)
{
	check(SourceComponent);
	SourceComponentName = SourceComponent->GetFName();
	SourceComponentClass = SourceComponent->GetClass();
	SourceComponentTypeSerializedIndex = -1;
	
	AActor* ComponentOwner = SourceComponent->GetOwner();
	if (ComponentOwner)
	{
		bool bFound = false;
		for (const UActorComponent* BlueprintCreatedComponent : ComponentOwner->BlueprintCreatedComponents)
		{
			if (BlueprintCreatedComponent)
			{
				if (BlueprintCreatedComponent == SourceComponent)
				{
					++SourceComponentTypeSerializedIndex;
					bFound = true;
					break;
				}
				else if (BlueprintCreatedComponent->GetClass() == SourceComponentClass)
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

	if (SourceComponent->CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
	{
		class FComponentPropertyWriter : public FObjectWriter
		{
		public:
			FComponentPropertyWriter(TArray<uint8>& InBytes)
				: FObjectWriter(InBytes)
			{
			}

			virtual bool ShouldSkipProperty(const UProperty* InProperty) const override
			{
				return (    InProperty->HasAnyPropertyFlags(CPF_Transient | CPF_ContainsInstancedReference | CPF_InstancedReference)
						|| !InProperty->HasAnyPropertyFlags(CPF_Edit | CPF_Interp));
			}

		} ComponentPropertyWriter(SavedProperties);

		SourceComponentClass->SerializeTaggedProperties(ComponentPropertyWriter, (uint8*)SourceComponent, SourceComponentClass, (uint8*)SourceComponent->GetArchetype());
	}
}

bool FComponentInstanceDataBase::MatchesComponent(const UActorComponent* Component) const
{
	bool bMatches = false;
	if (Component && Component->GetClass() == SourceComponentClass)
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
				for (const UActorComponent* BlueprintCreatedComponent : ComponentOwner->BlueprintCreatedComponents)
				{
					if (   BlueprintCreatedComponent
						&& (BlueprintCreatedComponent->GetClass() == SourceComponentClass)
						&& (++FoundSerializedComponentsOfType == SourceComponentTypeSerializedIndex))
					{
						bMatches = (BlueprintCreatedComponent == Component);
						break;
					}
				}
			}
		}
	}
	return bMatches;
}

void FComponentInstanceDataBase::ApplyToComponent(UActorComponent* Component)
{
	if (SavedProperties.Num() > 0)
	{
		class FComponentPropertyReader : public FObjectReader
		{
		public:
			FComponentPropertyReader(TArray<uint8>& InBytes)
				: FObjectReader(InBytes)
			{
			}
		} ComponentPropertyReader(SavedProperties);

		UObject* ArchetypeToSearch = Component->GetOuter()->GetArchetype();
		UClass* Class = Component->GetClass();

		Class->SerializeTaggedProperties(ComponentPropertyReader, (uint8*)Component, Class, nullptr);
	}
}

FComponentInstanceDataCache::FComponentInstanceDataCache(const AActor* Actor)
{
	if(Actor != NULL)
	{
		TInlineComponentArray<UActorComponent*> Components;
		Actor->GetComponents(Components);

		// Grab per-instance data we want to persist
		for (UActorComponent* Component : Components)
		{
			if (Component->IsCreatedByConstructionScript()) // Only cache data from 'created by construction script' components
			{
				FComponentInstanceDataBase* ComponentInstanceData = Component->GetComponentInstanceData();
				if (ComponentInstanceData)
				{
					check(!Component->GetComponentInstanceDataType().IsNone());
					TypeToDataMap.Add(Component->GetComponentInstanceDataType(), ComponentInstanceData);
				}
			}
			else if (Component->CreationMethod == EComponentCreationMethod::Instance)
			{
				// If the instance component is attached to a BP component we have to be prepared for the possibility that it will be deleted
				if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
				{
					if (SceneComponent->AttachParent && SceneComponent->AttachParent->IsCreatedByConstructionScript())
					{
						auto RootComponent = Actor->GetRootComponent();
						if (RootComponent)
						{
							InstanceComponentTransformToRootMap.Add(SceneComponent, SceneComponent->GetComponentTransform().GetRelativeTransform(RootComponent->GetComponentTransform()));
						}
					}
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
		TInlineComponentArray<UActorComponent*> Components;
		Actor->GetComponents(Components);

		// Apply per-instance data.
		for (UActorComponent* Component : Components)
		{
			if(Component->IsCreatedByConstructionScript()) // Only try and apply data to 'created by construction script' components
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
							ComponentInstanceData->ApplyToComponent(Component);
							break;
						}
					}
				}
			}
		}

		// Once we're done attaching, if we have any unattached instance components move them to the root
		for (auto InstanceTransformPair : InstanceComponentTransformToRootMap)
		{
			check(Actor->GetRootComponent());

			USceneComponent* SceneComponent = InstanceTransformPair.Key;
			if (SceneComponent && (SceneComponent->AttachParent == nullptr || SceneComponent->AttachParent->IsPendingKill()))
			{
				SceneComponent->AttachTo(Actor->GetRootComponent());
				SceneComponent->SetRelativeTransform(InstanceTransformPair.Value);
			}
		}
	}
}

void FComponentInstanceDataCache::FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	for (auto ComponentInstanceDataPair : TypeToDataMap)
	{
		if (ComponentInstanceDataPair.Value)
		{
			ComponentInstanceDataPair.Value->FindAndReplaceInstances(OldToNewInstanceMap);
		}
	}
	TArray<USceneComponent*> SceneComponents;
	InstanceComponentTransformToRootMap.GetKeys(SceneComponents);

	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (UObject* const* NewSceneComponent = OldToNewInstanceMap.Find(SceneComponent))
		{
			if (*NewSceneComponent)
			{
				InstanceComponentTransformToRootMap.Add(CastChecked<USceneComponent>(*NewSceneComponent), InstanceComponentTransformToRootMap.FindAndRemoveChecked(SceneComponent));
			}
			else
			{
				InstanceComponentTransformToRootMap.Remove(SceneComponent);
			}
		}
	}
}