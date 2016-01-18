// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequenceObjectReference.generated.h"


/**
 * An external reference to an level sequence object, resolvable through an arbitrary context.
 */
USTRUCT()
struct FLevelSequenceObjectReference
{
	/**
	 * Default construction to a null reference
	 */
	FLevelSequenceObjectReference() : ObjectPath() {}

	/**
	 * Generates a new reference to an object within a given context.
	 *
	 * @param InObject The object to generate a reference for (e.g. an AActor)
	 * @param InContext The object to use as a context for the generation (e.g. a UWorld)
	 */
	LEVELSEQUENCE_API FLevelSequenceObjectReference(UObject* InObject, UObject* InContext);

	GENERATED_BODY();

	/**
	 * Check whether this object reference is valid or not
	 */
	bool IsValid() const
	{
		return !ObjectPath.IsEmpty();
	}

	/**
	 * Resolve this reference within the specified context
	 *
	 * @return The object (usually an Actor or an ActorComponent).
	 */
	LEVELSEQUENCE_API UObject* Resolve(UObject* InContext) const;

	/**
	 * Equality comparator
	 */
	friend bool operator==(const FLevelSequenceObjectReference& A, const FLevelSequenceObjectReference& B)
	{
		if (A.ObjectId.IsValid() && A.ObjectId == B.ObjectId)
		{
			return true;
		}
		return A.ObjectPath == B.ObjectPath;
	}

	/**
	 * Serialization operator
	 */
	friend FArchive& operator<<(FArchive& Ar, FLevelSequenceObjectReference& Ref)
	{
		Ar << Ref.ObjectId;
		Ar << Ref.ObjectPath;
		return Ar;
	}

private:

	/** Primary method of resolution - object ID, stored as an annotation on the object in the world, resolvable through TLazyObjectPtr */
	FUniqueObjectGuid ObjectId;

	/** Secondary method of resolution - path to the object within the context */
	FString ObjectPath;
};

USTRUCT()
struct FLevelSequenceObjectReferenceMap
{
	/**
	 * Check whether this map has a binding for the specified object id
	 * @return true if this map contains a binding for the id, false otherwise
	 */
	bool HasBinding(const FGuid& ObjectId) const;

	/**
	 * Remove a binding for the specified ID
	 *
	 * @param ObjectId	The ID to remove
	 */
	void RemoveBinding(const FGuid& ObjectId);

	/**
	 * Create a binding for the specified ID
	 *
	 * @param ObjectId	The ID to associate the object with
	 * @param InObject	The object to associate
	 * @param InContext	A context in which InObject resides
	 */
	void CreateBinding(const FGuid& ObjectId, UObject* InObject, UObject* InContext);

	/**
	 * Resolve a binding for the specified ID using a given context
	 *
	 * @param ObjectId	The ID to associate the object with
	 * @param InContext	A context in which InObject resides
	 *
	 * @return The object if found, nullptr otherwise.
	 */
	UObject* ResolveBinding(const FGuid& ObjectId, UObject* InContext) const;

	/**
	 * Find an object ID that the specified object is bound with
	 *
	 * @param InObject	The object to find an ID for
	 * @param InContext	The context in which InObject resides
	 *
	 * @return The object's bound ID, or an empty FGuid
	 */
	FGuid FindBindingId(UObject* InObject, UObject* InContext) const;

public:

	GENERATED_BODY();

	/**
	 * Equality comparator required for proper UObject serialization (so we can compare against defaults)
	 */
	friend bool operator==(const FLevelSequenceObjectReferenceMap& A, const FLevelSequenceObjectReferenceMap& B)
	{
		return A.Map.OrderIndependentCompareEqual(B.Map);
	}

	/**
	 * Serialization function
	 */
	bool Serialize(FArchive& Ar);

	typedef TMap<FGuid, FLevelSequenceObjectReference> MapType;
	FORCEINLINE friend MapType::TIterator      begin(      FLevelSequenceObjectReferenceMap& Impl) { return begin(Impl.Map); }
	FORCEINLINE friend MapType::TConstIterator begin(const FLevelSequenceObjectReferenceMap& Impl) { return begin(Impl.Map); }
	FORCEINLINE friend MapType::TIterator      end  (      FLevelSequenceObjectReferenceMap& Impl) { return end(Impl.Map); }
	FORCEINLINE friend MapType::TConstIterator end  (const FLevelSequenceObjectReferenceMap& Impl) { return end(Impl.Map); }

private:
	
	// @todo sequencer: need support for UStruct keys in TMap so this can be a UPROPERTY
	TMap<FGuid, FLevelSequenceObjectReference> Map;
};

template<> struct TStructOpsTypeTraits<FLevelSequenceObjectReferenceMap> : public TStructOpsTypeTraitsBase
{
	enum { WithSerializer = true, WithIdenticalViaEquality = true };
};