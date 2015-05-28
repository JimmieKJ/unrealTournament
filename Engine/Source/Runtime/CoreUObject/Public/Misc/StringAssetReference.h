// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UObject;
struct FPropertyTag;

/**
 * A struct that contains a string reference to an asset, can be used to make soft references to assets
 */
struct COREUOBJECT_API FStringAssetReference
{
	/** Asset path */
	FString AssetLongPathname;
	
	FStringAssetReference()
	{
	}

	FStringAssetReference(const FStringAssetReference& Other)
		: AssetLongPathname(Other.AssetLongPathname)
	{
	}

	/**
	 * Construct from a path string
	 */
	FStringAssetReference(const FString& PathString)
		: AssetLongPathname(PathString)
	{
		if (AssetLongPathname == TEXT("None"))
		{
			AssetLongPathname = TEXT("");
		}
	}

	/**
	 * Construct from an existing object, will do some string processing
	 */
	FStringAssetReference(const UObject* InObject);

	/**
	 * Converts in to a string
	 */
	const FString& ToString() const 
	{
		return AssetLongPathname;
	}

	/**
	 * Attempts to load the asset.
	 * @return Loaded UObject, or null if the asset fails to load, or if the reference is not valid.
	 */
	UObject* TryLoad() const;

	/**
	 * Attempts to find a currently loaded object that matches this object ID
	 * @return Found UObject, or NULL if not currently loaded
	 */
	UObject* ResolveObject() const;

	/**
	 * Resets reference to point to NULL
	 */
	void Reset()
	{		
		AssetLongPathname = TEXT("");
	}
	
	/**
	 * Check if this could possibly refer to a real object, or was initialized to NULL
	 */
	bool IsValid() const
	{
		return AssetLongPathname.Len() > 0;
	}

	bool Serialize(FArchive& Ar);
	bool operator==(FStringAssetReference const& Other) const;
	bool operator!=(FStringAssetReference const& Other) const
	{
		return !(*this == Other);
	}
	void operator=(FStringAssetReference const& Other);
	bool ExportTextItem(FString& ValueStr, FStringAssetReference const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem( const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText );
	bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar);

	friend uint32 GetTypeHash(FStringAssetReference const& This)
	{
		return GetTypeHash(This.AssetLongPathname);
	}

	/** Code needed by AssetPtr to track rather object references should be rechecked */

	static int32 GetCurrentTag()
	{
		return CurrentTag.GetValue();
	}
	static int32 InvalidateTag()
	{
		return CurrentTag.Increment();
	}

	static FStringAssetReference GetOrCreateIDForObject(const UObject *Object);

	static void SetPackageNamesBeingDuplicatedForPIE(const TArray<FString>& InPackageNamesBeingDuplicatedForPIE);
	static void ClearPackageNamesBeingDuplicatedForPIE();

protected:

private:
	/**
	 * Fixes up this StringAssetReference to add or remove the PIE prefix depending on what is currently active
	 */
	void FixupForPIE();

	/** Global counter that determines when we need to re-search for GUIDs because more objects have been loaded **/
	static FThreadSafeCounter CurrentTag;
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
