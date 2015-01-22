// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

/**
 * Returns the UE4 version of the linker for this object.
 *
 * @return	the UE4 version of the engine's package file when this object
 *			was last saved, or GPackageFileUE4Version (current version) if
 *			this object does not have a linker, which indicates that
 *			a) this object is a native only class, or
 *			b) this object's linker has been detached, in which case it is already fully loaded
 */
int32 UObjectBaseUtility::GetLinkerUE4Version() const
{
	// Catch code errors like GetLinker()->GetLinkerUE4Version()
	check(!IsA(ULinkerLoad::StaticClass()));

	ULinkerLoad* Loader = GetLinker();

	// No linker.
	if( Loader == NULL )
	{
		// the _Linker reference is never set for the top-most UPackage of a package (the linker root), so if this object
		// is the linker root, find our loader in the global list.
		if( GetOutermost() == this )
		{
			Loader = ULinkerLoad::FindExistingLinkerForPackage(const_cast<UPackage*>(CastChecked<UPackage>((const UObject*)this)));
		}
	}

	if ( Loader != NULL )
	{
		// We have a linker so we can return its version.
		return Loader->UE4Ver();

	}
	else
	{
		// We don't have a linker associated as we e.g. might have been saved or had loaders reset, ...
		return GPackageFileUE4Version;
	}
}

int32 UObjectBaseUtility::GetLinkerCustomVersion(FGuid CustomVersionKey) const
{
	ULinkerLoad* Loader = GetLinker();

	// No linker.
	if( Loader == NULL )
	{
		// the _Linker reference is never set for the top-most UPackage of a package (the linker root), so if this object
		// is the linker root, find our loader in the global list.
		if( GetOutermost() == this )
		{
			Loader = ULinkerLoad::FindExistingLinkerForPackage(const_cast<UPackage*>(CastChecked<UPackage>((const UObject*)this)));
		}
	}

	if ( Loader != NULL )
	{
		// We have a linker so we can return its version.
		auto* CustomVersion = Loader->Summary.GetCustomVersionContainer().GetVersion(CustomVersionKey);
		return CustomVersion ? CustomVersion->Version : -1;
	}

	// We don't have a linker associated as we e.g. might have been saved or had loaders reset, ...
	// We must have a current version for this tag.
	auto* CustomVersion = FCustomVersionContainer::GetRegistered().GetVersion(CustomVersionKey);
	check(CustomVersion);
	return CustomVersion->Version;
}

/**
 * Returns the licensee version of the linker for this object.
 *
 * @return	the licensee version of the engine's package file when this object
 *			was last saved, or GPackageFileLicenseeVersion (current version) if
 *			this object does not have a linker, which indicates that
 *			a) this object is a native only class, or
 *			b) this object's linker has been detached, in which case it is already fully loaded
 */
int32 UObjectBaseUtility::GetLinkerLicenseeUE4Version() const
{
	ULinkerLoad* Loader = GetLinker();

	// No linker.
	if( Loader == NULL )
	{
		// the _Linker reference is never set for the top-most UPackage of a package (the linker root), so if this object
		// is the linker root, find our loader in the global list.
		if( GetOutermost() == this )
		{
			Loader = ULinkerLoad::FindExistingLinkerForPackage(const_cast<UPackage*>(CastChecked<UPackage>((const UObject*)this)));
		}
	}

	if ( Loader != NULL )
	{
		// We have a linker so we can return its version.
		return Loader->LicenseeUE4Ver();

	}
	else
	{
		// We don't have a linker associated as we e.g. might have been saved or had loaders reset, ...
		return GPackageFileLicenseeUE4Version;
	}
}
