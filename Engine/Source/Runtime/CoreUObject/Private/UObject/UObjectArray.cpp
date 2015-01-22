// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjArray.cpp: Unreal array of all objects
=============================================================================*/

#include "CoreUObjectPrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogUObjectArray, Log, All);

/** Global UObject array							*/
FUObjectArray GUObjectArray;

void FUObjectArray::AllocatePermanentObjectPool(int32 MaxObjectsNotConsideredByGC)
{
	// GObjFirstGCIndex is the index at which the garbage collector will start for the mark phase.
	ObjFirstGCIndex			= MaxObjectsNotConsideredByGC;

	// Presize array.
	check( ObjObjects.Num() == 0 );
	if( ObjFirstGCIndex )
	{
		ObjObjects.Reserve( ObjFirstGCIndex );
	}
	FWeakObjectPtr::Init(); // this adds a delete listener
}

void FUObjectArray::CloseDisregardForGC()
{
	OpenForDisregardForGC = false;
	GIsInitialLoad = false;
	// Make sure the first GC index matches the last non-GC index after DisregardForGC is closed
	ObjFirstGCIndex = ObjLastNonGCIndex + 1;
}

void FUObjectArray::AllocateUObjectIndex(UObjectBase* Object)
{
	int32 Index;
	check(Object->InternalIndex == INDEX_NONE);

	// Special non- garbage collectable range.
	if (OpenForDisregardForGC && DisregardForGCEnabled())
	{
		Index = ObjObjects.AddUninitialized();
		ObjLastNonGCIndex = Index;
		ObjFirstGCIndex = FMath::Max(ObjFirstGCIndex, Index + 1);
	}
	// Regular pool/ range.
	else
	{
		if(ObjAvailable.Num())
		{
			Index = ObjAvailable.Pop();
			check(ObjObjects[Index]==NULL);
		}
		else
		{
			Index = ObjObjects.AddUninitialized();
		}
		check(Index >= ObjFirstGCIndex);
	}
	// Add to global table.
	ObjObjects[Index] = Object;
	Object->InternalIndex = Index;
	for (int32 ListenerIndex = 0; ListenerIndex < UObjectCreateListeners.Num(); ListenerIndex++)
	{
		UObjectCreateListeners[ListenerIndex]->NotifyUObjectCreated(Object,Index);
	}
}

/**
 * Returns a UObject index to the global uobject array
 *
 * @param Object object to free
 */
void FUObjectArray::FreeUObjectIndex(UObjectBase* Object)
{
	int32 Index = Object->InternalIndex;
	ObjObjects[Index] = NULL;
	for (int32 ListenerIndex = 0; ListenerIndex < UObjectDeleteListeners.Num(); ListenerIndex++)
	{
		UObjectDeleteListeners[ListenerIndex]->NotifyUObjectDeleted(Object,Index);
	}
	if (Index > ObjLastNonGCIndex)  // you cannot safely recycle indicies in the non-GC range
	{
		ObjAvailable.Add(Index);
	}
}

/**
 * Adds a creation listener
 *
 * @param Listener listener to notify when an object is deleted
 */
void FUObjectArray::AddUObjectCreateListener(FUObjectCreateListener* Listener)
{
	check(!UObjectCreateListeners.Contains(Listener));
	UObjectCreateListeners.Add(Listener);
}

/**
 * Removes a listener for object creation
 *
 * @param Listener listener to remove
 */
void FUObjectArray::RemoveUObjectCreateListener(FUObjectCreateListener* Listener)
{
	int32 NumRemoved = UObjectCreateListeners.RemoveSingleSwap(Listener);
	check(NumRemoved==1);
}

/**
 * Checks whether object is part of permanent object pool.
 *
 * @param Listener listener to notify when an object is deleted
 */
void FUObjectArray::AddUObjectDeleteListener(FUObjectDeleteListener* Listener)
{
	check(!UObjectDeleteListeners.Contains(Listener));
	UObjectDeleteListeners.Add(Listener);
}

/**
 * removes a listener for object deletion
 *
 * @param Listener listener to remove
 */
void FUObjectArray::RemoveUObjectDeleteListener(FUObjectDeleteListener* Listener)
{
	UObjectDeleteListeners.RemoveSingleSwap(Listener);
}



/**
 * Checks if a UObject index is valid
 *
 * @param	Object object to test for validity
 * @return	true if this index is valid
 */
bool FUObjectArray::IsValid(const UObjectBase* Object) const 
{ 
	int32 Index = Object->InternalIndex;
	if( Index == INDEX_NONE )
	{
		UE_LOG(LogUObjectArray, Warning, TEXT("Object is not in global object array") );
		return false;
	}
	if( !ObjObjects.IsValidIndex(Index))
	{
		UE_LOG(LogUObjectArray, Warning, TEXT("Invalid object index %i"), Index );
		return false;
	}
	const UObjectBase *Slot = ObjObjects[Index];
	if( Slot == NULL )
	{
		UE_LOG(LogUObjectArray, Warning, TEXT("Empty slot") );
		return false;
	}
	if( Slot != Object )
	{
		UE_LOG(LogUObjectArray, Warning, TEXT("Other object in slot") );
		return false;
	}
	return true;
}

/**
 * Clears some internal arrays to get rid of false memory leaks
 */
void FUObjectArray::ShutdownUObjectArray()
{
	ObjObjects.Empty();
	ObjAvailable.Empty();
}

TArray<UObjectBase*>* FUObjectArray::GetObjectArrayForDebugVisualizers()
{
	return &GUObjectArray.ObjObjects;
}
