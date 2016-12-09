// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UObject/Class.h"

/**
 * A struct that contains a string reference to an asset on disk.
 * This can be used to make soft references to assets that are loaded on demand.
 * This is stored internally as a string of the form /package/path.assetname[.objectname]
 * If the MetaClass metadata is applied to a UProperty with this the UI will restrict to that type of asset.
 */
struct COREUOBJECT_API FStringAssetReference
{
	/** Asset path */
	DEPRECATED(4.9, "Please don't use AssetLongPathname directly. Instead use SetPath and ToString methods.")
	FString AssetLongPathname;
	
	FStringAssetReference()
	{
	}

	/** Construct from another asset reference */
	FStringAssetReference(const FStringAssetReference& Other)
	{
		SetPath(Other.ToString());
	}

	/** Construct from a path string */
	FStringAssetReference(FString PathString)
	{
		SetPath(MoveTemp(PathString));
	}

	/** Construct from an existing object in memory */
	FStringAssetReference(const UObject* InObject);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	
	/** Destructor, in deprecation block to avoid warnings */
	~FStringAssetReference() {}

	/** Returns string representation of reference, in form /package/path.assetname[.objectname] */
	FORCEINLINE const FString& ToString() const
	{
		return AssetLongPathname;		
	}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Returns /package/path, leaving off the asset name */
	FString GetLongPackageName() const
	{
		FString PackageName;
		ToString().Split(TEXT("."), &PackageName, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		return PackageName;
	}
	
	/** Returns assetname string, leaving off the /package/path part */
	const FString GetAssetName() const
	{
		FString AssetName;
		ToString().Split(TEXT("."), nullptr, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		return AssetName;
	}	

	/** Sets asset path of this reference based on a string path */
	void SetPath(FString Path);

	/**
	 * Attempts to load the asset, this will call LoadObject which can be very slow
	 *
	 * @return Loaded UObject, or nullptr if the reference is null or the asset fails to load
	 */
	UObject* TryLoad() const;

	/**
	 * Attempts to find a currently loaded object that matches this path
	 *
	 * @return Found UObject, or nullptr if not currently in memory
	 */
	UObject* ResolveObject() const;

	/** Resets reference to point to null */
	void Reset()
	{		
		SetPath(TEXT(""));
	}
	
	/** Check if this could possibly refer to a real object, or was initialized to null */
	bool IsValid() const
	{
		return ToString().Len() > 0;
	}

	/** Checks to see if this is initialized to null */
	bool IsNull() const
	{
		return ToString().Len() == 0;
	}

	/** Struct overrides */
	bool Serialize(FArchive& Ar);
	bool operator==(FStringAssetReference const& Other) const;
	bool operator!=(FStringAssetReference const& Other) const
	{
		return !(*this == Other);
	}
	FStringAssetReference& operator=(FStringAssetReference const& Other);
	bool ExportTextItem(FString& ValueStr, FStringAssetReference const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem( const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText );
	bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar);

	FORCEINLINE friend uint32 GetTypeHash(FStringAssetReference const& This)
	{
		return GetTypeHash(This.ToString());
	}

	/** Code needed by FAssetPtr internals */
	static int32 GetCurrentTag()
	{
		return CurrentTag.GetValue();
	}
	static int32 InvalidateTag()
	{
		return CurrentTag.Increment();
	}
	static FStringAssetReference GetOrCreateIDForObject(const UObject* Object);
	
	/** Enables special PIE path handling, to remove pie prefix as needed */
	static void SetPackageNamesBeingDuplicatedForPIE(const TArray<FString>& InPackageNamesBeingDuplicatedForPIE);
	
	/** Disables special PIE path handling */
	static void ClearPackageNamesBeingDuplicatedForPIE();

private:
	/** Fixes up this StringAssetReference to add or remove the PIE prefix depending on what is currently active */
	void FixupForPIE();

	/** Global counter that determines when we need to re-search based on path because more objects have been loaded **/
	static FThreadSafeCounter CurrentTag;

	/** Package names currently being duplicated, needed by FixupForPIE */
	static TArray<FString> PackageNamesBeingDuplicatedForPIE;
};

template<>
struct TStructOpsTypeTraits<FStringAssetReference> : public TStructOpsTypeTraitsBase
{
	enum 
	{
		WithZeroConstructor = true,
		WithSerializer = true,
		WithCopy = true,
		WithIdenticalViaEquality = true,
		WithExportTextItem = true,
		WithImportTextItem = true,
		WithSerializeFromMismatchedTag = true,
	};
};
