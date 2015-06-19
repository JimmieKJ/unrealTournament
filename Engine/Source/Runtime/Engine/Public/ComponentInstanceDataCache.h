// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UActorComponent;
class AActor;

/** At what point in the rerun construction script process is ApplyToActor being called for */
enum class ECacheApplyPhase
{
	PostSimpleConstructionScript,	// After the simple construction script has been run
	PostUserConstructionScript,		// After the user construction script has been run
};

/** Base class for component instance cached data of a particular type. */
class ENGINE_API FActorComponentInstanceData
{
public:
	FActorComponentInstanceData()
		: SourceComponentClass(nullptr)
		, SourceComponentTypeSerializedIndex(-1)
	{}

	FActorComponentInstanceData(const UActorComponent* SourceComponent);

	virtual ~FActorComponentInstanceData()
	{}

	/** Determines whether this component instance data matches the component */
	bool MatchesComponent(const UActorComponent* Component) const;

	/** Applies this component instance data to the supplied component */
	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase);

	/** Replaces any references to old instances during Actor reinstancing */
	virtual void FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap) { };

	virtual void AddReferencedObjects(FReferenceCollector& Collector);

	bool ContainsSavedProperties() const { return SavedProperties.Num() > 0; }

protected:
	/** The name of the source component */
	FName SourceComponentName;

	/** The class type of the source component */
	UClass* SourceComponentClass;

	/** The index of the source component in its owner's serialized array 
		when filtered to just that component type */
	int32 SourceComponentTypeSerializedIndex;

	TArray<uint8> SavedProperties;
};

/** 
 *	Cache for component instance data.
 *	Note, does not collect references for GC, so is not safe to GC if the cache is only reference to a UObject.
 */
class ENGINE_API FComponentInstanceDataCache
{
public:

	FComponentInstanceDataCache() {}

	/** Constructor that also populates cache from Actor */
	FComponentInstanceDataCache(const AActor* InActor);

	~FComponentInstanceDataCache();

	/** Iterates over an Actor's components and applies the stored component instance data to each */
	void ApplyToActor(AActor* Actor, const ECacheApplyPhase CacheApplyPhase) const;

	/** Iterates over components and replaces any object references with the reinstanced information */
	void FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	bool HasInstanceData() const { return TypeToDataMap.Num() > 0; }

	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	/** Map of data type name to data of that type */
	TMultiMap< FName, FActorComponentInstanceData* >	TypeToDataMap;

	TMap< USceneComponent*, FTransform > InstanceComponentTransformToRootMap;
};