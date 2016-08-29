// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CallbackDevice.h:  Allows the engine to do callbacks into the editor
=============================================================================*/

#pragma once

#include "ScopedCallback.h"

/**
 * Container struct that holds info about a redirector that was followed. 
 * Cannot contain a pointer to the actual UObjectRedirector as the object
 * will be GC'd before it is used
 */
struct FRedirection
{
	FString PackageFilename;
	FString RedirectorName;
	FString RedirectorPackageFilename;
	FString DestinationObjectName;

	bool operator==(const FRedirection& Other) const
	{
		return PackageFilename == Other.PackageFilename &&
			RedirectorName == Other.RedirectorName &&
			RedirectorPackageFilename == Other.RedirectorPackageFilename;
	}

	friend FArchive& operator<<(FArchive& Ar, FRedirection& Redir)
	{
		Ar << Redir.PackageFilename;
		Ar << Redir.RedirectorName;
		Ar << Redir.RedirectorPackageFilename;
		Ar << Redir.DestinationObjectName;
		return Ar;
	}
};

/**
 * Callback structure to respond to redirect-followed callbacks and store
 * the information
 */
class COREUOBJECT_API FRedirectCollector
{
private:
	
	/** Helper struct for string asset reference tracking */
	struct FPackagePropertyPair
	{
		FPackagePropertyPair() : bReferencedByEditorOnlyProperty(false) {}

		FPackagePropertyPair(const FName& InPackage, const FName& InProperty)
		: Package(InPackage)
		, Property(InProperty)
		, bReferencedByEditorOnlyProperty(false)
		//, PackageCache(NAME_None)
		{}

		bool operator==(const FPackagePropertyPair& Other) const
		{
			return Package == Other.Package &&
				Property == Other.Property;
		}

		const FName& GetCachedPackageName() const
		{
			return Package;
			/*if (PackageCache == NAME_None)
			{
				PackageCache = FName(*Package);
			}
			return PackageCache;*/
		}


		void SetPackage(const FName& InPackage)
		{
			Package = InPackage;
		}
		void SetProperty(const FName& InProperty)
		{
			Property = InProperty;
		}
		
		void SetReferencedByEditorOnlyProperty(bool InReferencedByEditorOnlyProperty)
		{
			bReferencedByEditorOnlyProperty = InReferencedByEditorOnlyProperty;
		}

		const FName& GetPackage() const
		{
			return Package;
		}
		const FName& GetProperty() const
		{
			return Property;
		}

		bool GetReferencedByEditorOnlyProperty() const
		{
			return bReferencedByEditorOnlyProperty;
		}

	private:
		FName Package;
		FName Property;
		bool bReferencedByEditorOnlyProperty;
		// mutable FName PackageCache;
	};

public:

	/**
	 * Responds to FCoreDelegates::RedirectorFollowed. Records all followed redirections
	 * so they can be cleaned later.
	 *
	 * @param InString Name of the package that pointed to the redirect
	 * @param InObject The UObjectRedirector that was followed
	 */
	void OnRedirectorFollowed( const FString& InString, UObject* InObject);
	
	/**
	 * Responds to FCoreDelegates::StringAssetReferenceLoaded. Just saves it off for later use.
	 * @param InString Name of the asset that was loaded
	 */
	void OnStringAssetReferenceLoaded(const FString& InString);

	/**
	 * Responds to FCoreDelegates::StringAssetReferenceSaved. Runs the string through the remap table.
	 * @param InString Name of the asset that will be saved
	 * @return actual value to save
	 */
	FString OnStringAssetReferenceSaved(const FString& InString);

	/**
	 * Load the string asset references to resolve them, add that to the remap table, and empty the array
	 * @param FilterPackage If set, only fixup references that were created by FilterPackage. If empty, clear all of them
	 */
	void ResolveStringAssetReference(FString FilterPackage = FString());

	/**
	 * Do we have any references to resolve.
	 * @return true if we have references to resolve
	 */
	bool HasAnyStringAssetReferencesToResolve() const
	{
		return StringAssetReferences.Num() > 0;
	}

	const TArray<FRedirection>& GetRedirections() const
	{
		return Redirections;
	}

	void LogTimers() const;

public:
	/** If not an empty string, only fixup redirects in this package */
	FString FileToFixup;

	/** A gathered list of all non-script referenced redirections */
	TArray<FRedirection> Redirections;

private:

	/** A gathered list string asset references , with the key being the string reference (GetPathName()) and the value equal to the package with the reference */
	TMultiMap<FName, FPackagePropertyPair> StringAssetReferences;

	/** When saving, apply this remapping to all string asset references */
	TMap<FString, FString> StringAssetRemap;
};

// global redirect collector callback structure
COREUOBJECT_API extern FRedirectCollector GRedirectCollector;

