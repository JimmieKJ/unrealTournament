// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PersistentObjectPtr.h: Template that is a base class for Lazy and Asset pointers
=============================================================================*/

#pragma once

/**
 * TPersistentObjectPtr is a template base class for FLazyObjectPtr and FAssetPtr
 */
template<class TObjectID>
class TPersistentObjectPtr
{
public:	

	/** NULL constructor **/
	FORCEINLINE TPersistentObjectPtr()
	{
		Reset();
	}

	/**
	 * Reset the lazy pointer back to the NULL state
	 */
	FORCEINLINE void Reset()
	{
		WeakPtr.Reset();
		ObjectID.Reset();
		TagAtLastTest = 0;
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	FORCEINLINE TPersistentObjectPtr(const TPersistentObjectPtr& Other)
	{
		(*this)=Other;
	}

	/**  
	 * Construct from a unique object identifier
	 * @param InObjectID a unique object identifier
	 */
	explicit FORCEINLINE TPersistentObjectPtr(const TObjectID& InObjectID)
		: WeakPtr()
		, TagAtLastTest(0)
		, ObjectID(InObjectID)
	{
	}

	/**  
	 * Copy from a unique object identifier
	 * @param ObjectID object identifier to create a weak pointer to
	 */
	FORCEINLINE void operator=(const TObjectID& InObjectID)
	{
		WeakPtr.Reset();
		ObjectID = InObjectID;
		TagAtLastTest = 0;
	}

	/**  
	 * Copy from an object pointer
	 * @param Object object to create a weak pointer to
	 */
	FORCEINLINE void operator=(const class UObject* Object)
	{
		if (Object)
		{
			ObjectID = TObjectID::GetOrCreateIDForObject(Object);
			WeakPtr = Object;
			TagAtLastTest = TObjectID::GetCurrentTag();
		}
		else
		{
			Reset();
		}
	}
	
	/**  
	 * Copy from an existing weak pointer, reserve IDs if required
	 * @param Other weak pointer to copy from
	 */
	FORCEINLINE void operator=(const FWeakObjectPtr& Other)
	{
		// If object exists need to make sure it gets registered properly in above function, if it doesn't exist just empty it
		const UObject *Object = Other.Get();
		*this = Object;
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	FORCEINLINE void operator=(const TPersistentObjectPtr<TObjectID>& Other)
	{
		WeakPtr = Other.WeakPtr;
		TagAtLastTest = Other.TagAtLastTest;
		ObjectID = Other.ObjectID;

	}
	
	/**
	 * Gets the unique object identifier associated with this lazy pointer. Valid even if pointer is not currently valid
	 * @return Unique ID for this object, or an invalid FUniqueObjectGuid if this pointer isn't set to anything
	 */
	FORCEINLINE const TObjectID& GetUniqueID() const
	{
		return ObjectID;
	}

	/**
	 * Non-const version of the above.
	 */
	FORCEINLINE TObjectID& GetUniqueID()
	{
		return ObjectID;
	}

	/**
	 * Dereference the lazy pointer, which may cause it to become valid again. Will not try to load pending outside of game thread
	 * @return NULL if this object is gone or the lazy pointer was NULL, otherwise a valid UObject pointer
	 */
	FORCEINLINE UObject* Get() const
	{
		UObject *Object = WeakPtr.Get();
		if (!Object && TObjectID::GetCurrentTag() != TagAtLastTest && ObjectID.IsValid())
		{
			Object = ObjectID.ResolveObject();
			WeakPtr = Object;
			TagAtLastTest = TObjectID::GetCurrentTag();

			// If this object is pending kill or otherwise invalid, this will return NULL as expected
			Object = WeakPtr.Get();
		}
		return Object;
	}

	/**
	 * Dereference the lazy pointer, which may cause it to become valid again. Will not try to load pending outside of game thread
	 * @param bEvenIfPendingKill, if this is true, pendingkill objects are considered valid
	 * @return NULL if this object is gone or the lazy pointer was NULL, otherwise a valid UObject pointer
	 */
	FORCEINLINE UObject* Get(bool bEvenIfPendingKill) const
	{
		UObject *Object = WeakPtr.Get(bEvenIfPendingKill);
		if (!Object && TObjectID::GetCurrentTag() != TagAtLastTest && ObjectID.IsValid())
		{
			Object = ObjectID.ResolveObject();
			WeakPtr = Object;
			TagAtLastTest = TObjectID::GetCurrentTag();

			// If this object is pending kill or otherwise invalid, this will return NULL as expected
			Object = WeakPtr.Get(bEvenIfPendingKill);
		}
		return Object;
	}

	/**  
	 * Dereference the lazy pointer.
	 */
	FORCEINLINE class UObject& operator*() const
	{
		return *Get();
	}

	/**  
	 * Dereference the lazy pointer.
	 */
	FORCEINLINE class UObject* operator->() const
	{
		return Get();
	}

	/**  
	 * Compare lazy pointers for equality. Only Serial Number matters for lazy pointers
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	**/
	FORCEINLINE bool operator==(const TPersistentObjectPtr& Other) const
	{
		return ObjectID == Other.ObjectID;
	}

	/**  
	 * Compare lazy pointers for inequality. Only Serial Number matters for lazy pointers
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	**/
	FORCEINLINE bool operator!=(const TPersistentObjectPtr& Other) const
	{
		return ObjectID != Other.ObjectID;
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return Get() == NULL && ObjectID.IsValid();
	}


	/**  
	 * Test if this points to a live UObject
	 * @return true if Get() would return a valid non-null pointer
	**/
	FORCEINLINE bool IsValid() const
	{
		return !!Get();
	}

	/**  
	 * Slightly different than !IsValid(), returns true if this used to point to a UObject, but doesn't any more and has not been assigned or reset in the mean time.
	 * @return true if this used to point at a real object but no longer does.
	**/
	FORCEINLINE bool IsStale() const
	{
		return WeakPtr.IsStale();
	}
	/**  
	 * Test if this can never point to a live UObject
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return Get() == NULL && !ObjectID.IsValid();
	}

	/** Hash function. */
	FORCEINLINE friend uint32 GetTypeHash(const TPersistentObjectPtr& Ptr)
	{
		return GetTypeHash(Ptr.ObjectID);
	}

private:

	/** Once the object has been noticed to be loaded, this is set to the object weak pointer **/
	mutable FWeakObjectPtr		WeakPtr;
	/** Compared to CurrentAnnotationTag and if they are not equal, a guid search will be performed **/
	mutable int32					TagAtLastTest;
	/** Guid for the object this pointer points to or will point to. **/
	TObjectID					ObjectID;
};

template <class TObjectID> struct TIsPODType<TPersistentObjectPtr<TObjectID> > { enum { Value = TIsPODType<TObjectID>::Value }; };
template <class TObjectID> struct TIsWeakPointerType<TPersistentObjectPtr<TObjectID> > { enum { Value = TIsWeakPointerType<FWeakObjectPtr>::Value }; };

