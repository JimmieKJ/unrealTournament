// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LazyObjectPtr.h: Lazy, guid-based pointer to UObject
=============================================================================*/

#pragma once

#include "PersistentObjectPtr.h"

/**
 * Wrapper structure for a GUID that uniquely identifies a UObject
 */
struct COREUOBJECT_API FUniqueObjectGuid
{
	FUniqueObjectGuid()
	{}

	FUniqueObjectGuid(const FGuid& InGuid)
		: Guid(InGuid)
	{}

	/**
	 * Reset the guid pointer back to the invalid state
	 */
	FORCEINLINE void Reset()
	{
		Guid.Invalidate();
	}

	/**
	 * Construct from an existing object
	 */
	FUniqueObjectGuid(const class UObject* InObject);

	/**
	 * Converts into a string
	 */
	FString ToString() const;

	/**
	 * Converts from a string
	 */
	void FromString(const FString& From);

	/**
	 * Fixes up this UniqueObjectID to add or remove the PIE prefix depending on what is currently active
	 */
	FUniqueObjectGuid FixupForPIE() const;

	/**
	 * Attempts to find a currently loaded object that matches this object ID
	 * @return Found UObject, or NULL if not currently loaded
	 */
	class UObject *ResolveObject() const;

	/**  
	 * Test if this can ever point to a live UObject
	 */
	FORCEINLINE bool IsValid() const
	{
		return Guid.IsValid();
	}

	FORCEINLINE bool operator==(const FUniqueObjectGuid &Other) const
	{
		return Guid == Other.Guid;
	}

	FORCEINLINE bool operator!=(const FUniqueObjectGuid &Other) const
	{
		return Guid != Other.Guid;
	}

	/**
	 * Annotation API
	 * @return true is this is a default.
	 */
	FORCEINLINE bool IsDefault()
	{
		return !IsValid(); // an default GUID is 0,0,0,0 and this is "invalid"
	}

	FORCEINLINE friend uint32 GetTypeHash(const FUniqueObjectGuid& ObjectGuid)
	{
		return GetTypeHash(ObjectGuid.Guid);
	}
	FORCEINLINE FGuid GetGuid() const
	{
		return Guid;
	}

	friend FArchive& operator<<(FArchive& Ar,FUniqueObjectGuid& ObjectGuid)
	{
		Ar << ObjectGuid.Guid;
		return Ar;
	}

	static int32 GetCurrentTag()
	{
		return CurrentAnnotationTag;
	}
	static int32 InvalidateTag()
	{
		return CurrentAnnotationTag++;
	}

	static FUniqueObjectGuid GetOrCreateIDForObject(const class UObject *Object);

private:
	FGuid Guid;

	/** Global counter that determines when we need to re-search for GUIDs because more objects have been loaded **/
	static int32 CurrentAnnotationTag;
};


template<> struct TIsPODType<FUniqueObjectGuid> { enum { Value = true }; };

/***
 * FLazyObjectPtr is a type of weak pointer to a UObject.
 * It will change back and forth between being valid or pending as the referenced object loads or unloads
 * It has no impact on if the object is garbage collected or not.
 * It can't be directly used across a network.
 *
 * This is useful for cross level references or any place you need to declare a reference to an object that may not be loaded
 **/

class FLazyObjectPtr : public TPersistentObjectPtr<FUniqueObjectGuid>
{
public:	
	/**
	 * Called by UObject::Serialize so that we can save / load the Guid possibly associated with an object
	 */
	COREUOBJECT_API static void PossiblySerializeObjectGuid(UObject *Object, FArchive& Ar);

	/**
	 * Called when entering PIE to prepare it for PIE-specific fixups
	 */
	COREUOBJECT_API static void ResetPIEFixups();

	/** NULL constructor **/
	FORCEINLINE FLazyObjectPtr()
	{
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	FORCEINLINE FLazyObjectPtr(const FLazyObjectPtr &Other)
	{
		(*this)=Other;
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	explicit FORCEINLINE FLazyObjectPtr(const UObject *Object)
	{
		(*this)=Object;
	}
	/**  
	 * Copy from an object pointer
	 * @param Object object to create a weak pointer to
	 */
	FORCEINLINE void operator=(const UObject *Object)
	{
		TPersistentObjectPtr<FUniqueObjectGuid>::operator=(Object);
	}

	/**  
	 * Copy from a lazy pointer
	 */
	FORCEINLINE void operator=(const FLazyObjectPtr &Other)
	{
		TPersistentObjectPtr<FUniqueObjectGuid>::operator=(Other);
	}

	/**  
	 * Copy from a unique object identifier
	 * @param ObjectID Object identifier to create a weak pointer to
	 */
	FORCEINLINE void operator=(const FUniqueObjectGuid &ObjectID)
	{
		TPersistentObjectPtr<FUniqueObjectGuid>::operator=(ObjectID);
	}
};

template <> struct TIsPODType<FLazyObjectPtr> { enum { Value = TIsPODType<TPersistentObjectPtr<FUniqueObjectGuid> >::Value }; };
template <> struct TIsWeakPointerType<FLazyObjectPtr> { enum { Value = TIsWeakPointerType<TPersistentObjectPtr<FUniqueObjectGuid> >::Value }; };

/***
* 
* TLazyObjectPtr is templatized version of the generic FLazyObjectPtr
* 
**/

template<class T=UObject>
class TLazyObjectPtr : private FLazyObjectPtr
{
public:
	/** NULL constructor **/
	FORCEINLINE TLazyObjectPtr()
	{
	}
	
	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	FORCEINLINE TLazyObjectPtr(const TLazyObjectPtr<T> &Other) :
		FLazyObjectPtr(Other)
	{
	}

	/**  
	 * Construct from an int (assumed to be NULL)
	 * @param Other int to create a reference to
	 */
	FORCEINLINE TLazyObjectPtr(TYPE_OF_NULL Other)
	{
		checkSlow(!Other); // we assume this is NULL
	}

	/**  
	 * Construct from an object pointer
	 * @param Object Object to create a lazy pointer to
	 */
	template<class U>
	FORCEINLINE TLazyObjectPtr(U* Object)
	{
		const T* TempObject = Object;
		FLazyObjectPtr::operator=(TempObject);
	}

	/**
	 * Reset the lazy pointer back to the NULL state
	 */
	FORCEINLINE void Reset()
	{
		FLazyObjectPtr::Reset();
	}

	/**  
	 * Copy from an object pointer
	 * @param Object Object to create a lazy pointer to
	 */
	template<class U>
	FORCEINLINE void operator=(const U *Object)
	{
		const T* TempObject = Object;
		FLazyObjectPtr::operator=(TempObject);
	}

	/**  
	 * Copy from a unique object identifier
	 * @param ObjectID Object identifier to create a weak pointer to
	 */
	FORCEINLINE void operator=(const FUniqueObjectGuid &ObjectID)
	{
		FLazyObjectPtr::operator=(ObjectID);
	}

	/**  
	 * Copy from a weak pointer
	 * @param Other Weak pointer to copy from
	 */
	FORCEINLINE void operator=(const TWeakObjectPtr<T> &Other)
	{
		FLazyObjectPtr::operator=(Other);
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	FORCEINLINE void operator=(const TLazyObjectPtr<T> &Other)
	{
		FLazyObjectPtr::operator=(Other);
	}

	/**  
	 * Copy from an int (assumed to be NULL)
	 * @param Other int to create a reference to
	 */
	FORCEINLINE void operator=(TYPE_OF_NULL Other)
	{
		checkSlow(!Other); // we assume this is NULL
		FLazyObjectPtr::Reset();
	}

	/**  
	 * Compare lazy pointers for equality
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	 */
	FORCEINLINE bool operator==(const TLazyObjectPtr<T> &Other) const
	{
		return FLazyObjectPtr::operator==(Other);
	}

	/**  
	 * Compare lazy pointers for equality
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	 */
	FORCEINLINE bool operator!=(const TLazyObjectPtr<T> &Other) const
	{
		return FLazyObjectPtr::operator!=(Other);
	}

	/**
	 * Gets the unique object identifier associated with this lazy pointer. Valid even if pointer is not currently valid
	 * @return Unique ID for this object, or an invalid FUniqueObjectID if this pointer isn't set to anything
	 */
	FUniqueObjectGuid GetUniqueID() const
	{
		return FLazyObjectPtr::GetUniqueID();
	}

	/**  
	 * Dereference the lazy pointer.
	 * @return NULL if this object is gone or the lazy pointer was NULL, otherwise a valid uobject pointer
	 */
	FORCEINLINE T *Get() const
	{
		return dynamic_cast<T*>(FLazyObjectPtr::Get());
	}

	/**  
	 * Dereference the lazy pointer.
	 */
	FORCEINLINE T & operator*() const
	{
		return *Get();
	}

	/**  
	 * Dereference the lazy pointer.
	 */
	FORCEINLINE T * operator->() const
	{
		return Get();
	}

	/**  
	 * Test if this points to a live UObject
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid() const
	{
		return FLazyObjectPtr::IsValid();
	}

	/**  
	 * Slightly different than !IsValid(), returns true if this used to point to a UObject, but doesn't any more and has not been assigned or reset in the mean time.
	 * @return true if this used to point at a real object but no longer does.
	**/
	FORCEINLINE bool IsStale() const
	{
		return FLazyObjectPtr::IsStale();
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 * @return true if this does not point to a real object, but could possibly
	**/
	FORCEINLINE bool IsPending() const
	{
		return FLazyObjectPtr::IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 * @return true if this is explicitly pointing to no object
	**/
	FORCEINLINE bool IsNull() const
	{
		return FLazyObjectPtr::IsNull();
	}

	/**  
	 * Dereference lazy pointer to see if it points somewhere valid.
	 */
	FORCEINLINE operator bool() const
	{
		return Get() != NULL;
	}

	/**  Compare for equality with a raw pointer **/
	template<class U>
	FORCEINLINE bool operator==(U* Other) const
	{
		return Get() == Other;
	}
	/**  Compare for equality with a raw pointer **/
	template<class U>
	FORCEINLINE bool operator==(const U* Other) const
	{
		return Get() == Other;
	}
	/**  Compare to NULL **/
	FORCEINLINE bool operator==(TYPE_OF_NULL Other) const
	{
		checkSlow(!Other); // we assume this is NULL
		return !IsValid();
	}

	/**  Compare for inequality with a raw pointer	**/
	template<class U>
	FORCEINLINE bool operator!=(U* Other) const
	{
		return Get() != Other;
	}
	/**  Compare for inequality with a raw pointer	**/
	template<class U>
	FORCEINLINE bool operator!=(const U* Other) const
	{
		return Get() != Other;
	}
	/**  Compare for inequality with NULL **/
	FORCEINLINE bool operator!=(TYPE_OF_NULL Other) const
	{
		checkSlow(!Other); // we assume this is NULL
		return IsValid();
	}

	/** Hash function. */
	FORCEINLINE friend uint32 GetTypeHash(const TLazyObjectPtr<T>& LazyObjectPtr)
	{
		return GetTypeHash(static_cast<const TPersistentObjectPtr<FUniqueObjectGuid>&>(LazyObjectPtr));
	}

	friend FArchive& operator<<( FArchive& Ar, TLazyObjectPtr<T>& LazyObjectPtr )
	{
		Ar << static_cast<FLazyObjectPtr&>(LazyObjectPtr);
		return Ar;
	}
};

template<class T> struct TIsPODType<TLazyObjectPtr<T> > { enum { Value = TIsPODType<FLazyObjectPtr>::Value }; };
template<class T> struct TIsWeakPointerType<TLazyObjectPtr<T> > { enum { Value = TIsWeakPointerType<FLazyObjectPtr>::Value }; };



