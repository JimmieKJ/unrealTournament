// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WeakObjectPtr.cpp: Weak pointer to UObject
=============================================================================*/

#include "CoreUObjectPrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeakObjectPtr, Log, All);

/*-----------------------------------------------------------------------------------------------------------
	Base serial number management. This is complicated by the need for thread safety and lock-free implementation
-------------------------------------------------------------------------------------------------------------*/

int32** GSerialNumberBlocksForDebugVisualizersRoot = 0;

/*-----------------------------------------------------------------------------------------------------------
	FWeakObjectPtr
-------------------------------------------------------------------------------------------------------------*/

/**  
 * Copy from an object pointer
 * @param Object object to create a weak pointer to
**/
void FWeakObjectPtr::operator=(const class UObject *Object)
{
	if (Object // && UObjectInitialized() we might need this at some point, but it is a speed hit we would prefer to avoid
		)
	{
		ObjectIndex = GUObjectArray.ObjectToIndex((UObjectBase*)Object);
		ObjectSerialNumber = GUObjectArray.AllocateSerialNumber(ObjectIndex);
		checkSlow(SerialNumbersMatch());
	}
	else
	{
		Reset();
	}
}

bool FWeakObjectPtr::IsValid(bool bEvenIfPendingKill, bool bThreadsafeTest) const
{
	// This is the external function, so we just pass through to the internal inlined method.
	return Internal_IsValid(bEvenIfPendingKill, bThreadsafeTest);
}


bool FWeakObjectPtr::IsStale(bool bEvenIfPendingKill, bool bThreadsafeTest) const
{
	if (ObjectSerialNumber == 0)
	{
		checkSlow(ObjectIndex == 0 || ObjectIndex == -1); // otherwise this is a corrupted weak pointer
		return false;
	}
	if (ObjectIndex < 0)
	{
		return true;
	}
	FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(ObjectIndex);
	if (!ObjectItem)
	{
		return true;
	}
	if (!SerialNumbersMatch(ObjectItem))
	{
		return true;
	}
	if (bThreadsafeTest)
	{
		return false;
	}
	return GUObjectArray.IsStale(ObjectItem, bEvenIfPendingKill);
}

UObject* FWeakObjectPtr::Get(/*bool bEvenIfPendingKill = false*/) const
{
	// Using a literal here allows the optimizer to remove branches later down the chain.
	return Internal_Get(false);
}

UObject* FWeakObjectPtr::Get(bool bEvenIfPendingKill) const
{
	return Internal_Get(bEvenIfPendingKill);
}

UObject* FWeakObjectPtr::GetEvenIfUnreachable() const
{
	UObject* Result = nullptr;
	if (Internal_IsValid(true, true))
	{
		FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(GetObjectIndex(), true);
		Result = static_cast<UObject*>(ObjectItem->Object);
	}
	return Result;
}


/**
 * Weak object pointer serialization.  Weak object pointers only have weak references to objects and
 * won't serialize the object when gathering references for garbage collection.  So in many cases, you
 * don't need to bother serializing weak object pointers.  However, serialization is required if you
 * want to load and save your object.
 */
FArchive& operator<<( FArchive& Ar, FWeakObjectPtr& WeakObjectPtr )
{
	// We never serialize our reference while the garbage collector is harvesting references
	// to objects, because we don't want weak object pointers to keep objects from being garbage
	// collected.  That would defeat the whole purpose of a weak object pointer!
	// However, when modifying both kinds of references we want to serialize and writeback the updated value.
	if( !Ar.IsObjectReferenceCollector() || Ar.IsModifyingWeakAndStrongReferences() )
	{
		// Downcast from UObjectBase to UObject
		UObject* Object = static_cast< UObject* >( WeakObjectPtr.Get(true) );

		Ar << Object;

		if( Ar.IsLoading() || Ar.IsModifyingWeakAndStrongReferences() )
		{
			WeakObjectPtr = Object;
		}
	}

	return Ar;
}

