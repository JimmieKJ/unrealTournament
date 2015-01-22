// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//
// Unreal networking serialization helpers
//

#pragma once
#include "Runtime/Online/OnlineSubsystem/Public/OnlineSubsystemTypes.h"
#include "OnlineReplStructs.generated.h"

/**
 * Wrapper for opaque type FUniqueNetId
 *
 * Makes sure that the opaque aspects of FUniqueNetId are properly handled/serialized 
 * over network RPC and actor replication
 */
USTRUCT()
struct FUniqueNetIdRepl
{
	GENERATED_USTRUCT_BODY()

	FUniqueNetIdRepl() :
		UniqueNetId(NULL)
	{}
	FUniqueNetIdRepl(TSharedRef<FUniqueNetId> InUniqueNetId) :
		UniqueNetId(InUniqueNetId)
	{
	}
	FUniqueNetIdRepl(TSharedPtr<FUniqueNetId> InUniqueNetId) :
		UniqueNetId(InUniqueNetId)
	{
	}

	~FUniqueNetIdRepl() {}

	/** Assignment operator */
	FUniqueNetIdRepl& operator=(const FUniqueNetIdRepl& Other)
	{
		if(&Other != this)
		{
			UniqueNetId = Other.UniqueNetId;
		}
		
		return *this;
	}

	/** Comparison operator */
	bool operator==(FUniqueNetIdRepl const& Other) const
	{
		// Both invalid structs or both valid and deep comparison equality
		bool bBothValid = IsValid() && Other.IsValid();
		bool bBothInvalid = !IsValid() && !Other.IsValid();
		return (bBothInvalid || (bBothValid && (*UniqueNetId == *Other.UniqueNetId)));
	}

	/** Comparison operator */
	bool operator!=(FUniqueNetIdRepl const& Other) const
	{
		return !(FUniqueNetIdRepl::operator==(Other));
	}

    /** Export contents of this struct as a string */
	bool ExportTextItem(FString& ValueStr, FUniqueNetIdRepl const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;

	/** Convert this value to a string */
	ENGINE_API FString ToString() const;

	/** Network serialization */
	ENGINE_API bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Serialization to any FArchive */
	ENGINE_API friend FArchive& operator<<( FArchive& Ar, FUniqueNetIdRepl& UniqueNetId);

	/** Serialization to any FArchive */
	bool Serialize(FArchive& Ar);

	/** Is the FUniqueNetId wrapped in this object valid */
	bool IsValid() const
	{
		return UniqueNetId.IsValid() && UniqueNetId->IsValid();
	}
	
	/** 
	 * Assign a unique id to this wrapper object
	 *
	 * @param InUniqueNetId id to associate
	 */
	void SetUniqueNetId(const TSharedPtr<FUniqueNetId>& InUniqueNetId)
	{
		UniqueNetId = InUniqueNetId;
	}

	/** @return unique id associated with this wrapper object */
	const TSharedPtr<FUniqueNetId>& GetUniqueNetId() const
	{
		return UniqueNetId;
	}

	/**
	 * Dereference operator returns a reference to the FUniqueNetId
	 */
	FUniqueNetId& operator*() const
	{
		return *UniqueNetId;
	}

	/**
	 * Arrow operator returns a pointer to this FUniqueNetId
	 */
	FUniqueNetId* operator->() const
	{
		return UniqueNetId.Get();
	}

private:

	/** UniqueId wrapped */
	TSharedPtr<class FUniqueNetId> UniqueNetId;
};

/** Specify net delta serializer support for the active skill cooldown array */
template<>
struct TStructOpsTypeTraits<FUniqueNetIdRepl> : public TStructOpsTypeTraitsBase
{
	enum 
	{
        // Can be copied via assignment operator
		WithCopy = true,
        // Requires custom serialization
		WithSerializer = true,
		// Requires custom net serialization
		WithNetSerializer = true,
		// Requires custom Identical operator for rep notifies in PostReceivedBunch()
		WithIdenticalViaEquality = true,
		// Export contents of this struct as a string (displayall, obj dump, etc)
		WithExportTextItem = true,
	};
};

/** Test harness for Unique Id replication */
ENGINE_API void TestUniqueIdRepl(class UWorld* InWorld);
