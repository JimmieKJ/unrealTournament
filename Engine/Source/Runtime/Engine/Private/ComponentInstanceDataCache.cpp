// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "ComponentInstanceDataCache.h"

FActorComponentInstanceData::FActorComponentInstanceData()
	: SourceComponentTemplate(nullptr)
	, SourceComponentTypeSerializedIndex(-1)
	, SourceComponentCreationMethod(EComponentCreationMethod::Native)
{
}

FActorComponentInstanceData::FActorComponentInstanceData(const UActorComponent* SourceComponent)
{
	check(SourceComponent);
	SourceComponentTemplate = SourceComponent->GetArchetype();
	SourceComponentCreationMethod = SourceComponent->CreationMethod;
	SourceComponentTypeSerializedIndex = -1;

	// UCS components can share the same template (e.g. an AddComponent node inside a loop), so we also cache their serialization index here (relative to the shared template) as a means for identification
	if(SourceComponentCreationMethod == EComponentCreationMethod::UserConstructionScript)
	{
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
					else if (   BlueprintCreatedComponent->CreationMethod == SourceComponentCreationMethod
						&& BlueprintCreatedComponent->GetArchetype() == SourceComponentTemplate)
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

	if (SourceComponent->IsEditableWhenInherited())
	{
		class FComponentPropertyWriter : public FObjectWriter
		{
		public:
			FComponentPropertyWriter(const UActorComponent* Component, TArray<uint8>& InBytes)
				: FObjectWriter(InBytes)
			{
				if (Component)
				{
					Component->GetUCSModifiedProperties(PropertiesToSkip);

					UClass* ComponentClass = Component->GetClass();
					ComponentClass->SerializeTaggedProperties(*this, (uint8*)Component, ComponentClass, (uint8*)Component->GetArchetype());
				}
			}

			virtual bool ShouldSkipProperty(const UProperty* InProperty) const override
			{
				return (	InProperty->HasAnyPropertyFlags(CPF_Transient | CPF_ContainsInstancedReference | CPF_InstancedReference)
						|| !InProperty->HasAnyPropertyFlags(CPF_Edit | CPF_Interp)
						|| PropertiesToSkip.Contains(InProperty));
			}

		private:
			TSet<const UProperty*> PropertiesToSkip;

		};

		FComponentPropertyWriter ComponentPropertyWriter(SourceComponent, SavedProperties);

		// Cache off the length of an array that will come from SerializeTaggedProperties that had no properties saved in to it.
		auto GetSizeOfEmptyArchive = [](const UActorComponent* DummyComponent) -> int32
		{
			TArray<uint8> NoWrittenPropertyReference;
			FComponentPropertyWriter NullWriter(nullptr, NoWrittenPropertyReference);
			UClass* ComponentClass = DummyComponent->GetClass();
			
			// By serializing the component with itself as its defaults we guarantee that no properties will be written out
			ComponentClass->SerializeTaggedProperties(NullWriter, (uint8*)DummyComponent, ComponentClass, (uint8*)DummyComponent);

			return NoWrittenPropertyReference.Num();
		};

		static const int32 SizeOfEmptyArchive = GetSizeOfEmptyArchive(SourceComponent);

		// SerializeTaggedProperties will always put a sentinel NAME_None at the end of the Archive. 
		// If that is the only thing in the buffer then empty it because we want to know that we haven't stored anything.
		if (SavedProperties.Num() == SizeOfEmptyArchive)
		{
			SavedProperties.Empty();
		}
	}
}

bool FActorComponentInstanceData::MatchesComponent(const UActorComponent* Component, const UObject* ComponentTemplate, const TMap<UActorComponent*, const UObject*>& ComponentToArchetypeMap) const
{
	bool bMatches = false;
	if (   Component
		&& Component->CreationMethod == SourceComponentCreationMethod
		&& (ComponentTemplate == SourceComponentTemplate || (GIsReinstancing && ComponentTemplate->GetFName() == SourceComponentTemplate->GetFName())))
	{
		if (SourceComponentCreationMethod != EComponentCreationMethod::UserConstructionScript)
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
					if (BlueprintCreatedComponent != nullptr && BlueprintCreatedComponent->CreationMethod == SourceComponentCreationMethod)
					{
						const UObject* BlueprintComponentTemplate = ComponentToArchetypeMap.FindChecked(BlueprintCreatedComponent);
						if (   (BlueprintComponentTemplate == SourceComponentTemplate || (GIsReinstancing && BlueprintComponentTemplate->GetFName() == SourceComponentTemplate->GetFName()))
							&& (++FoundSerializedComponentsOfType == SourceComponentTypeSerializedIndex))
						{
							bMatches = (BlueprintCreatedComponent == Component);
							break;
						}
					}
				}
			}
		}
	}
	return bMatches;
}

void FActorComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	// After the user construction script has run we will re-apply all the cached changes that do not conflict
	// with a change that the user construction script made.
	if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript && SavedProperties.Num() > 0)
	{
		Component->DetermineUCSModifiedProperties();

		class FComponentPropertyReader : public FObjectReader
		{
		public:
			FComponentPropertyReader(UActorComponent* InComponent, TArray<uint8>& InBytes)
				: FObjectReader(InBytes)
			{
				InComponent->GetUCSModifiedProperties(PropertiesToSkip);

				UClass* Class = InComponent->GetClass();
				Class->SerializeTaggedProperties(*this, (uint8*)InComponent, Class, nullptr);
			}

			virtual bool ShouldSkipProperty(const UProperty* InProperty) const override
			{
				return PropertiesToSkip.Contains(InProperty);
			}

			TSet<const UProperty*> PropertiesToSkip;

		} ComponentPropertyReader(Component, SavedProperties);

		if (Component->IsRegistered())
		{
			Component->ReregisterComponent();
		}
	}
}

void FActorComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SourceComponentTemplate);
}

FComponentInstanceDataCache::FComponentInstanceDataCache(const AActor* Actor)
{
	if (Actor != nullptr)
	{
		const bool bIsChildActor = Actor->IsChildActor();

		TInlineComponentArray<UActorComponent*> Components(Actor);

		ComponentsInstanceData.Reserve(Components.Num());

		// Grab per-instance data we want to persist
		for (UActorComponent* Component : Components)
		{
			if (bIsChildActor || Component->IsCreatedByConstructionScript()) // Only cache data from 'created by construction script' components
			{
				FActorComponentInstanceData* ComponentInstanceData = Component->GetComponentInstanceData();
				if (ComponentInstanceData)
				{
					ComponentsInstanceData.Add(ComponentInstanceData);
				}
			}
			else if (Component->CreationMethod == EComponentCreationMethod::Instance)
			{
				// If the instance component is attached to a BP component we have to be prepared for the possibility that it will be deleted
				if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
				{
					if (SceneComponent->GetAttachParent() && SceneComponent->GetAttachParent()->IsCreatedByConstructionScript())
					{
						InstanceComponentTransformToRootMap.Add(SceneComponent, SceneComponent->GetComponentTransform().GetRelativeTransform(Actor->GetRootComponent()->GetComponentTransform()));
					}
				}
			}
		}
	}
}

FComponentInstanceDataCache::~FComponentInstanceDataCache()
{
	for (FActorComponentInstanceData* ComponentData : ComponentsInstanceData)
	{
		delete ComponentData;
	}
}

void FComponentInstanceDataCache::ApplyToActor(AActor* Actor, const ECacheApplyPhase CacheApplyPhase) const
{
	if (Actor != nullptr)
	{
		const bool bIsChildActor = Actor->IsChildActor();

		TInlineComponentArray<UActorComponent*> Components(Actor);

		// Cache all archetype objects
		TMap<UActorComponent*, const UObject*> ComponentToArchetypeMap;
		ComponentToArchetypeMap.Reserve(Components.Num());

		for (UActorComponent* ComponentInstance : Components)
		{
			if (ComponentInstance && (bIsChildActor || ComponentInstance->IsCreatedByConstructionScript()))
			{
				ComponentToArchetypeMap.Add(ComponentInstance, ComponentInstance->GetArchetype());
			}
		}

		// Apply per-instance data.
		for (UActorComponent* ComponentInstance : Components)
		{
			if (ComponentInstance && (bIsChildActor || ComponentInstance->IsCreatedByConstructionScript())) // Only try and apply data to 'created by construction script' components
			{
				// Cache template here to avoid redundant calls in the loop below
				const UObject* ComponentTemplate = ComponentToArchetypeMap.FindChecked(ComponentInstance);

				for (FActorComponentInstanceData* ComponentInstanceData : ComponentsInstanceData)
				{
					if (	ComponentInstanceData
						&&	ComponentInstanceData->GetComponentClass() == ComponentTemplate->GetClass() // filter on class early to avoid unnecessary virtual and expensive tests
						&&	ComponentInstanceData->MatchesComponent(ComponentInstance, ComponentTemplate, ComponentToArchetypeMap))
					{
						ComponentInstanceData->ApplyToComponent(ComponentInstance, CacheApplyPhase);
						break;
					}
				}
			}
		}

		// Once we're done attaching, if we have any unattached instance components move them to the root
		for (auto InstanceTransformPair : InstanceComponentTransformToRootMap)
		{
			check(Actor->GetRootComponent());

			USceneComponent* SceneComponent = InstanceTransformPair.Key;
			if (SceneComponent && (SceneComponent->GetAttachParent() == nullptr || SceneComponent->GetAttachParent()->IsPendingKill()))
			{
				SceneComponent->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
				SceneComponent->SetRelativeTransform(InstanceTransformPair.Value);
			}
		}
	}
}

void FComponentInstanceDataCache::FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	for (FActorComponentInstanceData* ComponentInstanceData : ComponentsInstanceData)
	{
		if (ComponentInstanceData)
		{
			ComponentInstanceData->FindAndReplaceInstances(OldToNewInstanceMap);
		}
	}
	TArray<USceneComponent*> SceneComponents;
	InstanceComponentTransformToRootMap.GenerateKeyArray(SceneComponents);

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

void FComponentInstanceDataCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	TArray<USceneComponent*> SceneComponents;
	InstanceComponentTransformToRootMap.GenerateKeyArray(SceneComponents);

	Collector.AddReferencedObjects(SceneComponents);

	for (FActorComponentInstanceData* ComponentInstanceData : ComponentsInstanceData)
	{
		if (ComponentInstanceData)
		{
			ComponentInstanceData->AddReferencedObjects(Collector);
		}
	}
}