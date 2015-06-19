// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectArray.h: Unreal object array
=============================================================================*/

#ifndef __UOBJECTARRAY_H__
#define __UOBJECTARRAY_H__

/** Used to test for stale weak pointers in the debug visualizers **/
extern COREUOBJECT_API int32** GSerialNumberBlocksForDebugVisualizersRoot;

/***
* 
* FUObjectArray replaces the functionality of GObjObjects and UObject::Index
*
* Note the layout of this data structure is mostly to emulate the old behavior and minimize code rework during code restructure.
* Better data structures could be used in the future, for example maybe all that is needed is a TSet<UObject *>
* One has to be a little careful with this, especially with the GC optimization. I have seen spots that assume
* that non-GC objects come before GC ones during iteration.
*
**/


class COREUOBJECT_API FUObjectArray
{
public:

	/**
	 * Base class for UObjectBase create class listeners
	 */
	class FUObjectCreateListener
	{
	public:
		virtual ~FUObjectCreateListener() {}
		/**
		* Provides notification that a UObjectBase has been added to the uobject array
		 *
		 * @param Object object that has been destroyed
		 * @param Index	index of object that is being deleted
		 */
		virtual void NotifyUObjectCreated(const class UObjectBase *Object, int32 Index)=0;
	};

	/**
	 * Base class for UObjectBase delete class listeners
	 */
	class FUObjectDeleteListener
	{
	public:
		virtual ~FUObjectDeleteListener() {}

		/**
		 * Provides notification that a UObjectBase has been removed from the uobject array
		 *
		 * @param Object object that has been destroyed
		 * @param Index	index of object that is being deleted
		 */
		virtual void NotifyUObjectDeleted(const class UObjectBase *Object, int32 Index)=0;
	};

	/**
	 * Constructor, initializes to no permanent object pool
	 */
	FUObjectArray() :
		ObjFirstGCIndex(0),
		ObjLastNonGCIndex(INDEX_NONE),
		OpenForDisregardForGC(true)
	{
		FCoreDelegates::GetObjectArrayForDebugVisualizersDelegate().BindStatic(GetObjectArrayForDebugVisualizers);
	}

	/**
	 * Allocates and initializes the permanent object pool
	 *
	 * @param MaxObjectsNotConsideredByGC number of objects in the permanent object pool
	 */
	void AllocatePermanentObjectPool(int32 MaxObjectsNotConsideredByGC);

	/**
	* Reserves array memory to hold the specified number of objects
	*
	* @param MaxObjectsNotConsideredByGC number of objects in the permanent object pool
	*/
	void ReserveUObjectPool(int32 InNumObjects);

	/**
	 * Disables the disregard for GC optimization. Commandlets can't use it.
	 *
	 */
	void DisableDisregardForGC()
	{
		ObjFirstGCIndex = 0;
	}

	/**
	 * After the initial load, this closes the disregard pool so that new object are GC-able
	 *
	 */
	void CloseDisregardForGC();

	/**
	 * indicates if the disregard for GC optimization is active
	 *
	 * @return true if the first GC index is greater than zero; this indicates that the disregard for GC optimization is enabled
	 */
	bool DisregardForGCEnabled() const 
	{ 
		return ObjFirstGCIndex > 0; 
	}

	/**
	 * Adds a uobject to the global array which is used for uobject iteration
	 *
	 * @param	Object Object to allocate an index for
	 */
	void AllocateUObjectIndex(class UObjectBase* Object, bool bMergingThreads = false);

	/**
	 * Returns a UObject index top to the global uobject array
	 *
	 * @param Object object to free
	 */
	void FreeUObjectIndex(class UObjectBase* Object);

	/**
	 * Returns the index of a UObject. Be advised this is only for very low level use.
	 *
	 * @param Object object to get the index of
	 * @return index of this object
	 */
	FORCEINLINE int32 ObjectToIndex(const class UObjectBase* Object)
	{
		return Object->InternalIndex;
	}

	/**
	 * Returns the UObject corresponding to index. Be advised this is only for very low level use.
	 *
	 * @param Index index of object to return
	 * @return Object at this index
	 */
	FORCEINLINE class UObjectBase* IndexToObject(int32 Index)
	{
		check(Index >= 0);
		if (Index < ObjObjects.Num())
		{
			return (UObjectBase*)ObjObjects[Index];
		}
		return NULL;
	}

	FORCEINLINE class UObjectBase* IndexToObject(int32 Index, bool bEvenIfPendingKill)
	{
		UObjectBaseUtility* Object = static_cast<UObjectBaseUtility*>(IndexToObject(Index));
		if (Object && !bEvenIfPendingKill && Object->IsPendingKill())
		{
			Object = NULL;
		}
		return Object;
	}

	FORCEINLINE bool IsValid(int32 Index, bool bEvenIfPendingKill)
	{
		// This method assumes Index points to a valid object.
		UObjectBaseUtility* Object = static_cast<UObjectBaseUtility*>(IndexToObject(Index));
		if(Object == NULL)
		{
			return false;
		}
		return !Object->HasAnyFlags(RF_Unreachable) && (bEvenIfPendingKill || !Object->IsPendingKill());
	}

	FORCEINLINE bool IsStale(int32 Index, bool bEvenIfPendingKill)
	{
		// This method assumes Index points to a valid object.
		UObjectBaseUtility* Object = static_cast<UObjectBaseUtility*>(IndexToObject(Index));
		if(Object == NULL)
		{
			return true;
		}
		return Object->HasAnyFlags(RF_Unreachable) || (bEvenIfPendingKill && Object->IsPendingKill());
	}

	/**
	 * Adds a new listener for object creation
	 *
	 * @param Listener listener to notify when an object is deleted
	 */
	void AddUObjectCreateListener(FUObjectCreateListener* Listener);

	/**
	 * Removes a listener for object creation
	 *
	 * @param Listener listener to remove
	 */
	void RemoveUObjectCreateListener(FUObjectCreateListener* Listener);

	/**
	 * Adds a new listener for object deletion
	 *
	 * @param Listener listener to notify when an object is deleted
	 */
	void AddUObjectDeleteListener(FUObjectDeleteListener* Listener);

	/**
	 * Removes a listener for object deletion
	 *
	 * @param Listener listener to remove
	 */
	void RemoveUObjectDeleteListener(FUObjectDeleteListener* Listener);

	/**
	 * Checks if a UObject pointer is valid
	 *
	 * @param	Object object to test for validity
	 * @return	true if this index is valid
	 */
	bool IsValid(const UObjectBase* Object) const;

	/** Checks if the object index is valid. */
	FORCEINLINE bool IsValidIndex(const UObjectBase* Object) const 
	{ 
		return ObjObjects.IsValidIndex(Object->InternalIndex);
	}

	/**
	 * Returns true if this object is "disregard for GC"...same results as the legacy RF_DisregardForGC flag
	 *
	 * @param Object object to get for disregard for GC
	 * @return true if this object si disregard for GC
	 */
	FORCEINLINE bool IsDisregardForGC(const class UObjectBase* Object)
	{
		return Object->InternalIndex <= ObjLastNonGCIndex;
	}
	/**
	 * Returns the size of the global UObject array, some of these might be unused
	 *
	 * @return	the number of UObjects in the global array
	 */
	FORCEINLINE int32 GetObjectArrayNum() const 
	{ 
		return ObjObjects.Num();
	}

	/**
	 * Returns the size of the global UObject array minus the number of permanent objects
	 *
	 * @return	the number of UObjects in the global array
	 */
	FORCEINLINE int32 GetObjectArrayNumMinusPermanent() const 
	{ 
		return ObjObjects.Num() - (ObjLastNonGCIndex + 1);
	}

	/**
	 * Returns the number of permanent objects
	 *
	 * @return	the number of permanent objects
	 */
	FORCEINLINE int32 GetObjectArrayNumPermanent() const 
	{ 
		return ObjLastNonGCIndex + 1;
	}

#if WITH_EDITOR
	/**
	 * Returns the number of actual object indices that are claimed (the total size of the global object array minus
	 * the number of available object array elements
	 *
	 * @return	The number of objects claimed
	 */
	FORCEINLINE int32 GetObjectArrayNumMinusAvailable()
	{
		return ObjObjects.Num() - ObjAvailableCount.GetValue();
	}
#endif

	/**
	 * Clears some internal arrays to get rid of false memory leaks
	 */
	void ShutdownUObjectArray();


	/**
	 * Low level iterator
	 */
	class TIterator
	{
	public:
		enum EEndTagType
		{
			EndTag
		};

		/**
		 * Constructor
		 *
		 * @param	InArray				the array to iterate on
		 * @param	bOnlyGCedObjects	if true, skip all of the permanent objects
		 */
		TIterator( const FUObjectArray& InArray, bool bOnlyGCedObjects = false ) :	
			Array(InArray),
			Index(-1)
		{
			if (bOnlyGCedObjects)
			{
				Index = Array.ObjLastNonGCIndex;
			}
			Advance();
		}

		/**
		 * Constructor
		 *
		 * @param	InArray				the array to iterate on
		 * @param	bOnlyGCedObjects	if true, skip all of the permanent objects
		 */
		TIterator( EEndTagType, const TIterator& InIter ) :	
			Array (InIter.Array),
			Index(Array.ObjObjects.Num())
		{
		}

		/**
		 * Iterator advance
		 */
		FORCEINLINE void operator++()
		{
			Advance();
		}

		friend bool operator==(const TIterator& Lhs, const TIterator& Rhs) { return Lhs.Index == Rhs.Index; }
		friend bool operator!=(const TIterator& Lhs, const TIterator& Rhs) { return Lhs.Index != Rhs.Index; }

		/** Conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
		{ 
			return Array.ObjObjects.IsValidIndex(Index); 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

	protected:

		/**
		 * Dereferences the iterator with an ordinary name for clarity in derived classes
		 *
		 * @return	the UObject at the iterator
		 */
		FORCEINLINE UObjectBase* GetObject() const 
		{ 
			return (UObjectBase*)Array.ObjObjects[Index];
		}
		/**
		 * Iterator advance with ordinary name for clarity in subclasses
		 * @return	true if the iterator points to a valid object, false if iteration is complete
		 */
		FORCEINLINE bool Advance()
		{
			//@todo UE4 check this for LHS on Index on consoles
			while(++Index < Array.GetObjectArrayNum())
			{
				if (GetObject())
				{
					return true;
				}
			}
			return false;
		}
	private:
		/** the array that we are iterating on, probably always GetUObjectArray() */
		const FUObjectArray& Array;
		/** index of the current element in the object array */
		int32 Index;
	};

private:

	typedef TStaticIndirectArrayThreadSafeRead<UObjectBase, 8 * 1024 * 1024 /* Max 8M UObjects */, 16384 /* allocated in 64K/128K chunks */ > TUObjectArray;

	/**
	 * return the object array for use by debug visualizers
	 */
	static UObjectBase*** GetObjectArrayForDebugVisualizers();

	// note these variables are left with the Obj prefix so they can be related to the historical GObj versions

	/** First index into objects array taken into account for GC.							*/
	int32 ObjFirstGCIndex;
	/** Index pointing to last object created in range disregarded for GC.					*/
	int32 ObjLastNonGCIndex;
	/** If true this is the intial load and we should load objects int the disregarded for GC range.	*/
	int32 OpenForDisregardForGC;
	/** Array of all live objects.											*/
	TUObjectArray ObjObjects;
	/** Synchronization object for all live objects.											*/
	FCriticalSection ObjObjectsCritical;
	/** Available object indices.											*/
	TLockFreePointerList<int32> ObjAvailableList;
#if WITH_EDITOR
	/** Available object index count.										*/
	FThreadSafeCounter ObjAvailableCount;
#endif
	/**
	 * Array of things to notify when a UObjectBase is created
	 */
	TArray<FUObjectCreateListener* > UObjectCreateListeners;
	/**
	 * Array of things to notify when a UObjectBase is destroyed
	 */
	TArray<FUObjectDeleteListener* > UObjectDeleteListeners;
};

/** Global UObject allocator							*/
COREUOBJECT_API FUObjectArray& GetUObjectArray();

/**
	* Static version of IndexToObject for use with TWeakObjectPtr.
	*/
struct FIndexToObject
{
	static FORCEINLINE class UObjectBase* IndexToObject(int32 Index, bool bEvenIfPendingKill)
	{
		return GetUObjectArray().IndexToObject(Index, bEvenIfPendingKill);
	}
};

#endif	// __UOBJECTARRAY_H__
