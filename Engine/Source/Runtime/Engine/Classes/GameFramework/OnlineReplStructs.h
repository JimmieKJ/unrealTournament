// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//
// Unreal networking serialization helpers
//

#pragma once
#include "CoreOnline.h"
#include "OnlineReplStructs.generated.h"

class FJsonValue;

/**
 * Wrapper for opaque type FUniqueNetId
 *
 * Makes sure that the opaque aspects of FUniqueNetId are properly handled/serialized 
 * over network RPC and actor replication
 */
USTRUCT(BlueprintType)
struct FUniqueNetIdRepl
{
	GENERATED_USTRUCT_BODY()

	FUniqueNetIdRepl()
	{
	}

 	FUniqueNetIdRepl(const FUniqueNetIdWrapper& InWrapper)
 	: UniqueNetId(InWrapper.GetUniqueNetId())
 	{
 	}

	FUniqueNetIdRepl(const TSharedRef<const FUniqueNetId>& InUniqueNetId) :
		UniqueNetId(InUniqueNetId)
	{
	}

	FUniqueNetIdRepl(const TSharedPtr<const FUniqueNetId>& InUniqueNetId) :
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

	/** Import string contexts and try to map them into a unique id */
	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);

	/** Network serialization */
	ENGINE_API bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Serialization to any FArchive */
	ENGINE_API friend FArchive& operator<<( FArchive& Ar, FUniqueNetIdRepl& UniqueNetId);

	/** Serialization to any FArchive */
	bool Serialize(FArchive& Ar);
	
	/** Convert this value to a string */
	FString ToString() const
	{
		return IsValid() ? UniqueNetId->ToString() : TEXT("INVALID");
	}

	/** Convert this value to a string with additional information */
	virtual FString ToDebugString() const 
	{
		return IsValid() ? UniqueNetId->ToDebugString() : TEXT("INVALID");
	}

	/** Convert this unique id to a json value */
	TSharedRef<FJsonValue> ToJson() const;
	/** Create a unique id from a json string */
	void FromJson(const FString& InValue);

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
	void SetUniqueNetId(const TSharedPtr<const FUniqueNetId>& InUniqueNetId)
	{
		UniqueNetId = InUniqueNetId;
	}

	/** @return unique id associated with this wrapper object */
	const TSharedPtr<const FUniqueNetId>& GetUniqueNetId() const
	{
		return UniqueNetId;
	}

	/**
	 * Dereference operator returns a reference to the FUniqueNetId
	 */
	const FUniqueNetId& operator*() const
	{
		return *UniqueNetId;
	}

	/**
	 * Arrow operator returns a pointer to this FUniqueNetId
	 */
	const FUniqueNetId* operator->() const
	{
		return UniqueNetId.Get();
	}

	/**
	 * implicit cast operator to FUniqueNetIdWrapper
	 */
 	FORCEINLINE operator FUniqueNetIdWrapper() const
 	{
 		return FUniqueNetIdWrapper(UniqueNetId);
 	}

	friend inline uint32 GetTypeHash(FUniqueNetIdRepl const& Value)
	{
		if (Value.UniqueNetId.IsValid())
		{
			return (uint32)(*(*Value).GetBytes());
		}
		
		return 0;
	}

protected:
	TSharedPtr<const FUniqueNetId> UniqueNetId;

	/** Helper to create an FUniqueNetId from a string */
	void UniqueIdFromString(const FString& Contents);
};

/** Specify type trait support for various low level UPROPERTY overrides */
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
		// Import string contents as a unique id
		WithImportTextItem = true
	};
};

/** Test harness for Unique Id replication */
ENGINE_API void TestUniqueIdRepl(class UWorld* InWorld);
