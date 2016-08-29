// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	FUniqueObjectGuid FixupForPIE(int32 PlayInEditorID = GPlayInEditorID) const;

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
		return CurrentAnnotationTag.GetValue();
	}
	static int32 InvalidateTag()
	{
		return CurrentAnnotationTag.Increment();
	}

	static FUniqueObjectGuid GetOrCreateIDForObject(const class UObject *Object);

private:
	FGuid Guid;

	/** Global counter that determines when we need to re-search for GUIDs because more objects have been loaded **/
	static FThreadSafeCounter CurrentAnnotationTag;
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
	FORCEINLINE void operator=(const FUniqueObjectGuid &InObjectID)
	{
		TPersistentObjectPtr<FUniqueObjectGuid>::operator=(InObjectID);
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
#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	FORCEINLINE TLazyObjectPtr() = default;
	FORCEINLINE TLazyObjectPtr(const TLazyObjectPtr<T>& Other) = default;
	FORCEINLINE TLazyObjectPtr(TLazyObjectPtr<T>&& Other) = default;
	FORCEINLINE TLazyObjectPtr<T>& operator=(const TLazyObjectPtr<T>& Other) = default;
	FORCEINLINE TLazyObjectPtr<T>& operator=(TLazyObjectPtr<T>&& Other) = default;
#else
	/** NULL constructor **/
	FORCEINLINE TLazyObjectPtr()
	{
	}
	
	/**
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	FORCEINLINE TLazyObjectPtr(const TLazyObjectPtr<T>& Other) :
		FLazyObjectPtr(Other)
	{
	}

	/**
	* Assign from another lazy pointer
	* @param Other lazy pointer to copy from
	*/
	FORCEINLINE TLazyObjectPtr<T>& operator=(const TLazyObjectPtr<T>& Other)
	{
		FLazyObjectPtr::operator=(Other);
		return *this;
	}
#endif

	/**
	 * Construct from another lazy pointer with implicit upcasting allowed
	 * @param Other lazy pointer to copy from
	 */
	template<typename U, typename = typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type>
	FORCEINLINE TLazyObjectPtr(const TLazyObjectPtr<U>& Other) :
		FLazyObjectPtr((const FLazyObjectPtr&)Other)
	{
	}
	
	/**
	 * Assign from another lazy pointer with implicit upcasting allowed
	 * @param Other lazy pointer to copy from
	 */
	template<typename U>
	FORCEINLINE typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value, TLazyObjectPtr<T>&>::Type
		operator=(const TLazyObjectPtr<U>& Other)
	{
		FLazyObjectPtr::operator=((const FLazyObjectPtr&)Other);
		return *this;
	}

	/**
	 * Construct from an object pointer
	 * @param Object Object to create a lazy pointer to
	 */
	FORCEINLINE TLazyObjectPtr(T* Object)
	{
		FLazyObjectPtr::operator=(Object);
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
	FORCEINLINE void operator=(T* Object)
	{
		FLazyObjectPtr::operator=(Object);
	}

	/**
	 * Copy from a unique object identifier
	 * WARNING: this doesn't check the type of the object is correct,
	 * because the object corresponding to this ID may not even be loaded!
	 * @param ObjectID Object identifier to create a lazy pointer to
	 */
	FORCEINLINE void operator=(const FUniqueObjectGuid& InObjectID)
	{
		FLazyObjectPtr::operator=(InObjectID);
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
	FORCEINLINE T* Get() const
	{
		// there are cases where a TLazyObjectPtr can get an object of the wrong type assigned to it which are difficult to avoid
		// e.g. operator=(const FUniqueObjectGuid& ObjectID)
		// "WARNING: this doesn't check the type of the object is correct..."
		return dynamic_cast<T*>(FLazyObjectPtr::Get());
	}

	/**
	 * Dereference the lazy pointer.
	 */
	FORCEINLINE T& operator*() const
	{
		return *Get();
	}

	/**
	 * Dereference the lazy pointer.
	 */
	FORCEINLINE T* operator->() const
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
	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	/** Hash function. */
	FORCEINLINE friend uint32 GetTypeHash(const TLazyObjectPtr<T>& LazyObjectPtr)
	{
		return GetTypeHash(static_cast<const FLazyObjectPtr&>(LazyObjectPtr));
	}

	friend FArchive& operator<<(FArchive& Ar, TLazyObjectPtr<T>& LazyObjectPtr)
	{
		Ar << static_cast<FLazyObjectPtr&>(LazyObjectPtr);
		return Ar;
	}
};

// The reason these aren't inside the class (above) is because Visual Studio 2012-2013 crashes when compiling them :D

// Compare with another TLazyObjectPtr of related type
template<typename T, typename U>
FORCEINLINE bool operator==(const TLazyObjectPtr<T>& Lhs, const TLazyObjectPtr<U>& Rhs)
{
	static_assert(TPointerIsConvertibleFromTo<U, T>::Value || TPointerIsConvertibleFromTo<T, U>::Value, "Unable to compare TLazyObjectPtr - types are incompatible");
	return (const FLazyObjectPtr&)Lhs == (const FLazyObjectPtr&)Rhs;
}
template<typename T, typename U>
FORCEINLINE bool operator!=(const TLazyObjectPtr<T>& Lhs, const TLazyObjectPtr<U>& Rhs)
{
	static_assert(TPointerIsConvertibleFromTo<U, T>::Value || TPointerIsConvertibleFromTo<T, U>::Value, "Unable to compare TLazyObjectPtr - types are incompatible");
	return (const FLazyObjectPtr&)Lhs != (const FLazyObjectPtr&)Rhs;
}

/** Compare for equality with a raw pointer **/
template<typename T, typename U>
FORCEINLINE bool operator==(const TLazyObjectPtr<T>& Lhs, const U* Rhs)
{
	static_assert(TPointerIsConvertibleFromTo<U, T>::Value || TPointerIsConvertibleFromTo<T, U>::Value, "Unable to compare TLazyObjectPtr with raw pointer - types are incompatible");
	return Lhs.Get() == Rhs;
}
template<typename T, typename U>
FORCEINLINE bool operator==(const U* Lhs, const TLazyObjectPtr<T>& Rhs)
{
	static_assert(TPointerIsConvertibleFromTo<U, T>::Value || TPointerIsConvertibleFromTo<T, U>::Value, "Unable to compare TLazyObjectPtr with raw pointer - types are incompatible");
	return Lhs == Rhs.Get();
}

/** Compare to null **/
template<typename T>
FORCEINLINE bool operator==(const TLazyObjectPtr<T>& Lhs, TYPE_OF_NULLPTR)
{
	return !Lhs.IsValid();
}
template<typename T>
FORCEINLINE bool operator==(TYPE_OF_NULLPTR, const TLazyObjectPtr<T>& Rhs)
{
	return !Rhs.IsValid();
}

/** Compare for inequality with a raw pointer	**/
template<typename T, typename U>
FORCEINLINE bool operator!=(const TLazyObjectPtr<T>& Lhs, const U* Rhs)
{
	static_assert(TPointerIsConvertibleFromTo<U, T>::Value || TPointerIsConvertibleFromTo<T, U>::Value, "Unable to compare TLazyObjectPtr - types are incompatible");
	return Lhs.Get() != Rhs;
}
template<typename T, typename U>
FORCEINLINE bool operator!=(const U* Lhs, const TLazyObjectPtr<T>& Rhs)
{
	static_assert(TPointerIsConvertibleFromTo<U, T>::Value || TPointerIsConvertibleFromTo<T, U>::Value, "Unable to compare TLazyObjectPtr - types are incompatible");
	return Lhs != Rhs.Get();
}

/** Compare for inequality with null **/
template<typename T>
FORCEINLINE bool operator!=(const TLazyObjectPtr<T>& Lhs, TYPE_OF_NULLPTR)
{
	return Lhs.IsValid();
}
template<typename T>
FORCEINLINE bool operator!=(TYPE_OF_NULLPTR, const TLazyObjectPtr<T>& Rhs)
{
	return Rhs.IsValid();
}


template<class T> struct TIsPODType<TLazyObjectPtr<T> > { enum { Value = TIsPODType<FLazyObjectPtr>::Value }; };
template<class T> struct TIsWeakPointerType<TLazyObjectPtr<T> > { enum { Value = TIsWeakPointerType<FLazyObjectPtr>::Value }; };
