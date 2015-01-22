// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AssetPtr.h: Pointer to UObject asset, keeps extra information so that it is robust to missing assets.
=============================================================================*/

#pragma once

#include "AutoPointer.h"
#include "StringAssetReference.h"

/***
 * FAssetPtr is a type of weak pointer to a UObject.
 * It will change back and forth between being valid or pending as the referenced object loads or unloads
 * It has no impact on if the object is garbage collected or not.
 * It can't be directly used across a network.
 * Implicit conversion is disabled because that can cause memory leaks by reserving IDs that will never be used.
 *
 * This is useful for cross level references or any place you need to declare a reference to an object that may not be loaded
 **/
class FAssetPtr : public TPersistentObjectPtr<FStringAssetReference>
{
public:	
	/** NULL constructor **/
	FORCEINLINE FAssetPtr()
	{
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	FORCEINLINE FAssetPtr(const FAssetPtr &Other)
	{
		(*this)=Other;
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	explicit FORCEINLINE FAssetPtr(const UObject *Object)
	{
		(*this)=Object;
	}
	/**  
	 * Copy from an object pointer
	 * @param Object object to create a weak pointer to
	 */
	FORCEINLINE void operator=(const UObject *Object)
	{
		TPersistentObjectPtr<FStringAssetReference>::operator=(Object);
	}

	/**  
	 * Copy from an asset pointer
	 */
	FORCEINLINE void operator=(const FAssetPtr &Other)
	{
		TPersistentObjectPtr<FStringAssetReference>::operator=(Other);
	}

	/**  
	 * Copy from a unique object identifier
	 * @param ObjectID Object identifier to create a weak pointer to
	 */
	FORCEINLINE void operator=(const FStringAssetReference &ObjectID)
	{
		TPersistentObjectPtr<FStringAssetReference>::operator=(ObjectID);
	}
};

template <> struct TIsPODType<FAssetPtr> { enum { Value = TIsPODType<TPersistentObjectPtr<FStringAssetReference> >::Value }; };
template <> struct TIsWeakPointerType<FAssetPtr> { enum { Value = TIsWeakPointerType<TPersistentObjectPtr<FStringAssetReference> >::Value }; };

/***
* 
* TAssetPtr is templatized version of the generic FAssetPtr
* 
**/

template<class T=UObject>
class TAssetPtr : private FAssetPtr
{
public:
	/** NULL constructor **/
	FORCEINLINE TAssetPtr()
	{
	}
	
	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	FORCEINLINE TAssetPtr(const TAssetPtr<T> &Other) :
		FAssetPtr(Other)
	{
	}

	/**  
	 * Construct from an int (assumed to be NULL)
	 * @param Other int to create a reference to
	 */
	FORCEINLINE TAssetPtr(TYPE_OF_NULL Other)
	{
		checkSlow(!Other); // we assume this is NULL
	}

	/**  
	 * Construct from an object pointer
	 * @param Object object to create a reference to
	**/
	FORCEINLINE TAssetPtr(const T *Object) :
		FAssetPtr(Object)
	{
	}

	/**
	 * Reset the lazy pointer back to the NULL state
	 */
	FORCEINLINE void Reset()
	{
		FAssetPtr::Reset();
	}

	/**  
	 * Copy from an object pointer
	 * @param Object Object to create a lazy pointer to
	 */
	template<class U>
	FORCEINLINE void operator=(const U *Object)
	{
		const T* TempObject = Object;
		FAssetPtr::operator=(TempObject);
	}

	/**  
	 * Copy from a unique object identifier
	 * @param ObjectID Object identifier to create a weak pointer to
	 */
	FORCEINLINE void operator=(const FStringAssetReference &ObjectID)
	{
		FAssetPtr::operator=(ObjectID);
	}

	/**  
	 * Copy from a weak pointer
	 * @param Other Weak pointer to copy from
	 */
	FORCEINLINE void operator=(const TWeakObjectPtr<T> &Other)
	{
		FAssetPtr::operator=(Other);
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	FORCEINLINE void operator=(const TAssetPtr<T> &Other)
	{
		FAssetPtr::operator=(Other);
	}

	/**  
	 * Copy from an int (assumed to be NULL)
	 * @param Other int to create a reference to
	 */
	FORCEINLINE void operator=(TYPE_OF_NULL Other)
	{
		checkSlow(!Other); // we assume this is NULL
		FAssetPtr::Reset();
	}

	/**  
	 * Compare lazy pointers for equality
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	 */
	FORCEINLINE bool operator==(const TAssetPtr<T> &Other) const
	{
		return FAssetPtr::operator==(Other);
	}

	/**  
	 * Compare lazy pointers for equality
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	 */
	FORCEINLINE bool operator!=(const TAssetPtr<T> &Other) const
	{
		return FAssetPtr::operator!=(Other);
	}

	/**
	 * Gets the unique object identifier associated with this lazy pointer. Valid even if pointer is not currently valid
	 * @return Unique ID for this object, or an invalid FStringAssetReference if this pointer isn't set to anything
	 */
	FStringAssetReference GetUniqueID() const
	{
		return FAssetPtr::GetUniqueID();
	}

	/**  
	 * Dereference the lazy pointer.
	 * @return NULL if this object is gone or the lazy pointer was NULL, otherwise a valid uobject pointer
	 */
	FORCEINLINE T *Get() const
	{
		return dynamic_cast<T*>(FAssetPtr::Get());
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
		return FAssetPtr::IsValid();
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return FAssetPtr::IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return FAssetPtr::IsNull();
	}

	/**  
	 * Returns the StringAssetReference that is wrapped by this AssetPtr
	 */
	FORCEINLINE const FStringAssetReference& ToStringReference() const
	{
		return FAssetPtr::GetUniqueID();
	}

	/**  
	 * Dereference lazy pointer to see if it points somewhere valid.
	 */
	FORCEINLINE operator bool() const
	{
		return Get() != NULL;
	}

	/**  Compare for equality with a raw pointer **/
	FORCEINLINE bool operator==(T* Other) const
	{
		return Get() == Other;
	}
	/**  Compare for equality with a raw pointer **/
	FORCEINLINE bool operator==(const T* Other) const
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
	FORCEINLINE bool operator!=(T* Other) const
	{
		return Get() != Other;
	}
	/**  Compare for inequality with a raw pointer	**/
	FORCEINLINE bool operator!=(const T* Other) const
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
	FORCEINLINE friend uint32 GetTypeHash(const TAssetPtr<T>& AssetPtr)
	{
		return GetTypeHash(static_cast<const TPersistentObjectPtr<FStringAssetReference>&>(AssetPtr));
	}

	friend FArchive& operator<<( FArchive& Ar, TAssetPtr<T>& AssetPtr )
	{
		Ar << static_cast<FAssetPtr&>(AssetPtr);
		return Ar;
	}
};

template<class T> struct TIsPODType<TAssetPtr<T> > { enum { Value = TIsPODType<FAssetPtr>::Value }; };
template <class T> struct TIsWeakPointerType<TAssetPtr<T> > { enum { Value = TIsWeakPointerType<FAssetPtr>::Value }; };

template<class TClass>
class TAssetSubclassOf : private FAssetPtr
{
public:
	/** NULL constructor **/
	FORCEINLINE TAssetSubclassOf()
	{
	}
	
	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	template<class TClassA>
	FORCEINLINE TAssetSubclassOf(const TAssetSubclassOf<TClassA> &Other) :
		FAssetPtr(Other)
	{
		TClass* Test = (TClassA*)0; // verify that TClassA derives from TClass
	}

	/**  
	 * Construct from an class pointer
	 * @param From Class to create a reference to
	**/
	FORCEINLINE TAssetSubclassOf(const UClass* From) :
		FAssetPtr(From)
	{
	}

	/**  
	 * Construct from an int (assumed to be NULL)
	 * @param Other int to create a reference to
	 */
	FORCEINLINE TAssetSubclassOf(TYPE_OF_NULL Other)
	{
		checkSlow(!Other); // we assume this is NULL
	}

	/**
	 * Reset the lazy pointer back to the NULL state
	 */
	FORCEINLINE void Reset()
	{
		FAssetPtr::Reset();
	}

	/**  
	 * Copy from an object pointer
	 * @param Object Object to create a lazy pointer to
	 */
	FORCEINLINE void operator=(const UClass* From)
	{
		FAssetPtr::operator=(From);
	}

	/**  
	 * Copy from a unique object identifier
	 * @param ObjectID Object identifier to create a weak pointer to
	 */
	FORCEINLINE void operator=(const FStringAssetReference &ObjectID)
	{
		FAssetPtr::operator=(ObjectID);
	}

	/**  
	 * Copy from a weak pointer
	 * @param Other Weak pointer to copy from
	 */
	template<class TClassA>
	FORCEINLINE void operator=(const TWeakObjectPtr<TClassA> &Other)
	{
		UClass* Test = (TClassA*)0; //verify that TClassA derives from UClass
		FAssetPtr::operator=(Other);
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	template<class TClassA>
	FORCEINLINE void operator=(const TAssetSubclassOf<TClassA> &Other)
	{
		TClass* Test = (TClassA*)0; // verify that TClassA derives from TClass
		FAssetPtr::operator=(Other);
	}

	/**  
	 * Copy from an int (assumed to be NULL)
	 * @param Other int to create a reference to
	 */
	FORCEINLINE void operator=(TYPE_OF_NULL Other)
	{
		checkSlow(!Other); // we assume this is NULL
		FAssetPtr::Reset();
	}

	/**  
	 * Compare lazy pointers for equality
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	 */
	FORCEINLINE bool operator==(const FAssetPtr &Other) const
	{
		return FAssetPtr::operator==(Other);
	}

	/**  
	 * Compare lazy pointers for equality
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	 */
	FORCEINLINE bool operator!=(const FAssetPtr &Other) const
	{
		return FAssetPtr::operator!=(Other);
	}

	/**
	 * Gets the unique object identifier associated with this lazy pointer. Valid even if pointer is not currently valid
	 * @return Unique ID for this object, or an invalid FStringAssetReference if this pointer isn't set to anything
	 */
	FStringAssetReference GetUniqueID() const
	{
		return FAssetPtr::GetUniqueID();
	}

	/**  
	 * Dereference the lazy pointer.
	 * @return NULL if this object is gone or the lazy pointer was NULL, otherwise a valid uobject pointer
	 */
	FORCEINLINE UClass *Get() const
	{
		UClass* Class = dynamic_cast<UClass*>(FAssetPtr::Get());
		if (!Class || !Class->IsChildOf(TClass::StaticClass()))
		{
			return NULL;
		}
		return Class;
	}

	/**  
	 * Dereference the lazy pointer.
	 */
	FORCEINLINE UClass & operator*() const
	{
		return *Get();
	}

	/**  
	 * Dereference the lazy pointer.
	 */
	FORCEINLINE UClass * operator->() const
	{
		return Get();
	}

	/**  
	 * Test if this points to a live UObject
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid() const
	{
		return (NULL != Get());
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return FAssetPtr::IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return FAssetPtr::IsNull();
	}

	/**  
	 * Returns the StringAssetReference that is wrapped by this AssetPtr
	 */
	FORCEINLINE const FStringAssetReference& ToStringReference() const
	{
		return FAssetPtr::GetUniqueID();
	}

	/**  
	 * Dereference lazy pointer to see if it points somewhere valid.
	 */
	FORCEINLINE operator bool() const
	{
		return Get() != NULL;
	}

	/**  Compare for equality with a raw pointer **/
	FORCEINLINE bool operator==(UClass* Other) const
	{
		return Get() == Other;
	}
	/**  Compare for equality with a raw pointer **/
	FORCEINLINE bool operator==(const UClass* Other) const
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
	FORCEINLINE bool operator!=(UClass* Other) const
	{
		return Get() != Other;
	}
	/**  Compare for inequality with a raw pointer	**/
	FORCEINLINE bool operator!=(const UClass* Other) const
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
	FORCEINLINE friend uint32 GetTypeHash(const TAssetSubclassOf<TClass>& AssetPtr)
	{
		return GetTypeHash(static_cast<const TPersistentObjectPtr<FStringAssetReference>&>(AssetPtr));
	}

	friend FArchive& operator<<( FArchive& Ar, TAssetSubclassOf<TClass>& AssetPtr )
	{
		Ar << static_cast<FAssetPtr&>(AssetPtr);
		return Ar;
	}
};

template<class T> struct TIsPODType<TAssetSubclassOf<T> > { enum { Value = TIsPODType<FAssetPtr>::Value }; };
template <class T> struct TIsWeakPointerType<TAssetSubclassOf<T> > { enum { Value = TIsWeakPointerType<FAssetPtr>::Value }; };