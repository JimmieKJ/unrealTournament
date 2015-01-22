// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UActorComponent;
class AActor;

/** Base class for component instance cached data of a particular type. */
class ENGINE_API FComponentInstanceDataBase
{
public:
	FComponentInstanceDataBase()
		: SourceComponentTypeSerializedIndex(-1)
	{}

	FComponentInstanceDataBase(const UActorComponent* SourceComponent);

	virtual ~FComponentInstanceDataBase()
	{}

	virtual bool MatchesComponent(const UActorComponent* Component) const;

protected:
	/** The name of the source component */
	FName SourceComponentName;

	/** The index of the source component in its owner's serialized array 
		when filtered to just that component type */
	int32 SourceComponentTypeSerializedIndex;
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

	/** Util to iterate over components and apply data to each */
	void ApplyToActor(AActor* Actor) const;

	bool HasInstanceData() const { return TypeToDataMap.Num() > 0; }

private:
	/** Map of data type name to data of that type */
	TMultiMap< FName, FComponentInstanceDataBase* >	TypeToDataMap;
};