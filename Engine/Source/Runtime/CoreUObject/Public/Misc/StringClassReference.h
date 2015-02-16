// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StringAssetReference.h"

/**
 * A struct that contains a string reference to a class, can be used to make soft references to classes
 */
struct COREUOBJECT_API FStringClassReference : public FStringAssetReference
{
	FStringClassReference()
	{ }

	FStringClassReference(FStringClassReference const& Other)
		: FStringAssetReference(Other)
	{ }

	/**
	 * Construct from a path string
	 */
	FStringClassReference(const FString& PathString)
		: FStringAssetReference(PathString)
	{ }

	/**
	 * Construct from an existing class, will do some string processing
	 */
	FStringClassReference(const UClass* InClass)
		: FStringAssetReference(InClass)
	{ }

	/**
	* Attempts to load the class.
	* @return Loaded UObject, or null if the class fails to load, or if the reference is not valid.
	*/
	template<typename T>
	UClass* TryLoadClass() const
	{
		if ( IsValid() )
		{
			return LoadClass<T>(nullptr, *ToString(), nullptr, LOAD_None, nullptr);
		}

		return nullptr;
	}

	/**
	 * Attempts to find a currently loaded object that matches this object ID
	 * @return Found UClass, or NULL if not currently loaded
	 */
	UClass* ResolveClass() const;

	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);

	static FStringClassReference GetOrCreateIDForClass(const UClass *InClass);

private:
	/* Forbid creation for UObject. This class is for UClass only. Use FStringAssetReference instead. */
	FStringClassReference(const UObject* InObject) { }

	/* Forbidden. This class is for UClass only. Use FStringAssetReference instead. */
	static FStringAssetReference GetOrCreateIDForObject(const UObject *Object);
};

template<>
struct TStructOpsTypeTraits<FStringClassReference> : public TStructOpsTypeTraitsBase
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
