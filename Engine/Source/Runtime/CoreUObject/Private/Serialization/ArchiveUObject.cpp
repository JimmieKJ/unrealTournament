// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogArchiveUObject, Log, All);

/*----------------------------------------------------------------------------
	FArchiveUObject.
----------------------------------------------------------------------------*/
/**
 * Lazy object pointer serialization.  Lazy object pointers only have weak references to objects and
 * won't serialize the object when gathering references for garbage collection.  So in many cases, you
 * don't need to bother serializing lazy object pointers.  However, serialization is required if you
 * want to load and save your object.
 */
FArchive& FArchiveUObject::operator<<( class FLazyObjectPtr& LazyObjectPtr )
{
	FArchive& Ar = *this;
	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want weak object pointers to keep objects from being garbage
	// collected.  That would defeat the whole purpose of a weak object pointer!
	// However, when modifying both kinds of references we want to serialize and writeback the updated value.
	// We only want to write the modified value during reference fixup if the data is loaded
	if( !IsObjectReferenceCollector() || IsModifyingWeakAndStrongReferences() )
	{
		// Downcast from UObjectBase to UObject
		UObject* Object = static_cast< UObject* >( LazyObjectPtr.Get() );

		Ar << Object;

		if( IsLoading() || (Object && IsModifyingWeakAndStrongReferences()) )
		{
			LazyObjectPtr = Object;
		}
	}
	return Ar;
}

/**
 * Asset pointer serialization.  Asset pointers only have weak references to objects and
 * won't serialize the object when gathering references for garbage collection.  So in many cases, you
 * don't need to bother serializing asset pointers.  However, serialization is required if you
 * want to load and save your object.
 */
FArchive& FArchiveUObject::operator<<( class FAssetPtr& AssetPtr )
{
	FArchive& Ar = *this;
	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want weak object pointers to keep objects from being garbage
	// collected.  That would defeat the whole purpose of a weak object pointer!
	// However, when modifying both kinds of references we want to serialize and writeback the updated value.
	// We only want to write the modified value during reference fixup if the data is loaded
	if( !IsObjectReferenceCollector() || IsModifyingWeakAndStrongReferences() )
	{
		// Downcast from UObjectBase to UObject
		UObject* Object = static_cast< UObject* >( AssetPtr.Get() );

		Ar << Object;

		if( IsLoading() || (Object && IsModifyingWeakAndStrongReferences()) )
		{
			AssetPtr = Object;
		}
	}
	return Ar;
}

FArchive& FArchiveUObject::operator<<(struct FStringAssetReference& Value)
{
	*this << Value.AssetLongPathname;
	return *this;
}

/*----------------------------------------------------------------------------
	FObjectAndNameAsStringProxyArchive.
----------------------------------------------------------------------------*/
/**
 * Serialize the given UObject* as an FString
 */
FArchive& FObjectAndNameAsStringProxyArchive::operator<<(class UObject*& Obj)
{
	if (IsLoading())
	{
		// load the path name to the object
		FString LoadedString;
		InnerArchive << LoadedString;
		// look up the object by fully qualified pathname
		Obj = FindObject<UObject>(NULL, *LoadedString, false);
		// If we couldn't find it, and we want to load it, do that
		if((Obj == NULL) && bLoadIfFindFails)
		{
			Obj = LoadObject<UObject>(NULL, *LoadedString);
		}

		return InnerArchive;
	}
	else
	{
		// save out the fully qualified object name
		FString SavedString(Obj->GetPathName());
		return InnerArchive << SavedString;
	}
}

