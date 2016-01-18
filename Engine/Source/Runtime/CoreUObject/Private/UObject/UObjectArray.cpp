// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnObjArray.cpp: Unreal array of all objects
=============================================================================*/

#include "CoreUObjectPrivate.h"

DEFINE_LOG_CATEGORY_STATIC(LogUObjectArray, Log, All);

void FUObjectArray::AllocateObjectPool(int32 MaxUObjects, int32 MaxObjectsNotConsideredByGC)
{
	check(IsInGameThread());

	// GObjFirstGCIndex is the index at which the garbage collector will start for the mark phase.
	ObjFirstGCIndex = MaxObjectsNotConsideredByGC;

	// Pre-size array.
	check(ObjObjects.Num() == 0);
	ObjObjects.PreAllocate(MaxUObjects);
}

void FUObjectArray::CloseDisregardForGC()
{
	check(IsInGameThread());

	OpenForDisregardForGC = false;
	GIsInitialLoad = false;
	// Make sure the first GC index matches the last non-GC index after DisregardForGC is closed
	ObjFirstGCIndex = ObjLastNonGCIndex + 1;
}

void FUObjectArray::AllocateUObjectIndex(UObjectBase* Object, bool bMergingThreads /*= false*/)
{
	int32 Index = INDEX_NONE;
	check(Object->InternalIndex == INDEX_NONE || bMergingThreads);

	// Special non- garbage collectable range.
	if (OpenForDisregardForGC && DisregardForGCEnabled())
	{
		// Disregard from GC pool is only available from the game thread, at least for now
		check(IsInGameThread());
		Index = ObjObjects.AddSingle();
		ObjLastNonGCIndex = Index;
		ObjFirstGCIndex = FMath::Max(ObjFirstGCIndex, Index + 1);
	}
	// Regular pool/ range.
	else
	{		
		int32* AvailableIndex = ObjAvailableList.Pop();
		if (AvailableIndex)
		{
#if UE_GC_TRACK_OBJ_AVAILABLE
			const int32 AvailableCount = ObjAvailableCount.Decrement();
			checkSlow(AvailableCount >= 0);
#endif
			Index = (int32)(uintptr_t)AvailableIndex;
			check(ObjObjects[Index].Object==nullptr);
		}
		else
		{
#if THREADSAFE_UOBJECTS
			FScopeLock ObjObjectsLock(&ObjObjectsCritical);
#else
			check(IsInGameThread());
#endif
			Index = ObjObjects.AddSingle();
		}
		check(Index >= ObjFirstGCIndex);
	}
	// Add to global table.
	if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&ObjObjects[Index].Object, Object, NULL) != NULL) // we use an atomic operation to check for unexpected concurrency, verify alignment, etc
	{
		UE_LOG(LogUObjectArray, Fatal, TEXT("Unexpected concurency while adding new object"));
	}
	IndexToObject(Index)->ResetSerialNumberAndFlags();
	Object->InternalIndex = Index;
	//  @todo: threading: lock UObjectCreateListeners
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
	// This should only be happening on the game thread (GC runs only on game thread when it's freeing objects)
	check(IsInGameThread());

	int32 Index = Object->InternalIndex;
	// At this point no two objects exist with the same index so no need to lock here
	if (FPlatformAtomics::InterlockedCompareExchangePointer((void**)&ObjObjects[Index].Object, NULL, Object) == NULL) // we use an atomic operation to check for unexpected concurrency, verify alignment, etc
	{
		UE_LOG(LogUObjectArray, Fatal, TEXT("Unexpected concurency while adding new object"));
	}

	// @todo: threading: delete listeners should be locked while we're doing this
	for (int32 ListenerIndex = 0; ListenerIndex < UObjectDeleteListeners.Num(); ListenerIndex++)
	{
		UObjectDeleteListeners[ListenerIndex]->NotifyUObjectDeleted(Object, Index);
	}
	// You cannot safely recycle indicies in the non-GC range
	// No point in filling this list when doing exit purge. Nothing should be allocated afterwards anyway.
	if (Index > ObjLastNonGCIndex && !GExitPurge)  
	{
		IndexToObject(Index)->ResetSerialNumberAndFlags();
		ObjAvailableList.Push((int32*)(uintptr_t)Index);
#if UE_GC_TRACK_OBJ_AVAILABLE
		ObjAvailableCount.Increment();
#endif
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
#if THREADSAFE_UOBJECTS
	FScopeLock UObjectDeleteListenersLock(&UObjectDeleteListenersCritical);
#endif
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
#if THREADSAFE_UOBJECTS
	FScopeLock UObjectDeleteListenersLock(&UObjectDeleteListenersCritical);
#endif
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
	const FUObjectItem& Slot = ObjObjects[Index];
	if( Slot.Object == NULL )
	{
		UE_LOG(LogUObjectArray, Warning, TEXT("Empty slot") );
		return false;
	}
	if( Slot.Object != Object )
	{
		UE_LOG(LogUObjectArray, Warning, TEXT("Other object in slot") );
		return false;
	}
	return true;
}

int32 FUObjectArray::AllocateSerialNumber(int32 Index)
{
	FUObjectItem* ObjectItem = IndexToObject(Index);
	checkSlow(ObjectItem);

	volatile int32 *SerialNumberPtr = &ObjectItem->SerialNumber;
	int32 SerialNumber = *SerialNumberPtr;
	if (!SerialNumber)
	{
		SerialNumber = MasterSerialNumber.Increment();
		UE_CLOG(SerialNumber <= START_SERIAL_NUMBER, LogUObjectArray, Fatal, TEXT("UObject serial numbers overflowed (trying to allocate serial number %d)."), SerialNumber);
		int32 ValueWas = FPlatformAtomics::InterlockedCompareExchange((int32*)SerialNumberPtr, SerialNumber, 0);
		if (ValueWas != 0)
		{
			// someone else go it first, use their value
			SerialNumber = ValueWas;
		}
	}
	checkSlow(SerialNumber > START_SERIAL_NUMBER);
	return SerialNumber;
}

/**
 * Clears some internal arrays to get rid of false memory leaks
 */
void FUObjectArray::ShutdownUObjectArray()
{
}

UObjectBase*** FUObjectArray::GetObjectArrayForDebugVisualizers()
{
	return GUObjectArray.ObjObjects.GetRootBlockForDebuggerVisualizers();
}
