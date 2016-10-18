// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	 * Construct from a unique object identifier
	 * @param ObjectID Object identifier to create a weak pointer to
	**/
	explicit FORCEINLINE FAssetPtr(const FStringAssetReference &ObjectID)
		: TPersistentObjectPtr<FStringAssetReference>(ObjectID)
	{
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
	 * Synchronously load (if necessary) and return the asset object represented by this asset ptr
	 */
	UObject* LoadSynchronous()
	{
		UObject* Asset = Get();
		if (Asset == nullptr && IsPending())
		{
			Asset = GetUniqueID().TryLoad();
			*this = Asset;
		}
		return Asset;
	}

	using TPersistentObjectPtr<FStringAssetReference>::operator=;
};

template <> struct TIsPODType<FAssetPtr> { enum { Value = TIsPODType<TPersistentObjectPtr<FStringAssetReference> >::Value }; };
template <> struct TIsWeakPointerType<FAssetPtr> { enum { Value = TIsWeakPointerType<TPersistentObjectPtr<FStringAssetReference> >::Value }; };

/***
* 
* TAssetPtr is templatized version of the generic FAssetPtr
* 
**/

template<class T=UObject>
class TAssetPtr
{
	template <class U>
	friend class TAssetPtr;

public:
	/** NULL constructor **/
	FORCEINLINE TAssetPtr()
	{
	}
	
	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	template <class U, class = typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type>
	FORCEINLINE TAssetPtr(const TAssetPtr<U>& Other)
		: AssetPtr(Other.AssetPtr)
	{
	}

	/**  
	 * Construct from an object pointer
	 * @param Object object to create a reference to
	**/
	FORCEINLINE TAssetPtr(const T* Object)
		: AssetPtr(Object)
	{
	}

	/**  
	 * Construct from a unique object identifier
	 * @param ObjectID Object identifier to create a weak pointer to
	**/
	explicit FORCEINLINE TAssetPtr(const FStringAssetReference &ObjectID)
		: AssetPtr(ObjectID)
	{
	}

	/**
	 * Reset the lazy pointer back to the NULL state
	 */
	FORCEINLINE void Reset()
	{
		AssetPtr.Reset();
	}

	/**  
	 * Copy from an object pointer
	 * @param Object Object to create a lazy pointer to
	 */
	FORCEINLINE void operator=(const T* Object)
	{
		AssetPtr = Object;
	}

	/**  
	 * Copy from a unique object identifier
	 * @param ObjectID Object identifier to create a weak pointer to
	 */
	FORCEINLINE void operator=(const FStringAssetReference &ObjectID)
	{
		AssetPtr = ObjectID;
	}

	/**  
	 * Copy from a weak pointer
	 * @param Other Weak pointer to copy from
	 */
	template <class U>
	FORCEINLINE typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type
		operator=(const TWeakObjectPtr<U>& Other)
	{
		AssetPtr = Other;
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	template <class U>
	FORCEINLINE typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type
		operator=(const TAssetPtr<U>& Other)
	{
		AssetPtr = Other.AssetPtr;
	}

	/**  
	 * Compare lazy pointers for equality
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	 */
	FORCEINLINE friend bool operator==(const TAssetPtr& Lhs, const TAssetPtr& Rhs)
	{
		return Lhs.AssetPtr == Rhs.AssetPtr;
	}

	/**  
	 * Compare lazy pointers for inequality
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	 */
	FORCEINLINE friend bool operator!=(const TAssetPtr& Lhs, const TAssetPtr& Rhs)
	{
		return Lhs.AssetPtr != Rhs.AssetPtr;
	}

	/**
	 * Gets the unique object identifier associated with this lazy pointer. Valid even if pointer is not currently valid
	 * @return Unique ID for this object, or an invalid FStringAssetReference if this pointer isn't set to anything
	 */
	FStringAssetReference GetUniqueID() const
	{
		return AssetPtr.GetUniqueID();
	}

	/**  
	 * Dereference the lazy pointer.
	 * @return NULL if this object is gone or the lazy pointer was NULL, otherwise a valid uobject pointer
	 */
	FORCEINLINE T* Get() const
	{
		return dynamic_cast<T*>(AssetPtr.Get());
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
	 * Synchronously load (if necessary) and return the asset object represented by this asset ptr
	 */
	T* LoadSynchronous()
	{
		UObject* Asset = AssetPtr.Get();
		if (Asset == nullptr && IsPending())
		{
			Asset = AssetPtr.LoadSynchronous();
		}
		return Cast<T>(Asset);
	}

	/**  
	 * Test if this points to a live UObject
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid() const
	{
		return AssetPtr.IsValid();
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return AssetPtr.IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return AssetPtr.IsNull();
	}

	/**  
	 * Returns the StringAssetReference that is wrapped by this AssetPtr
	 */
	FORCEINLINE const FStringAssetReference& ToStringReference() const
	{
		return AssetPtr.GetUniqueID();
	}

	FORCEINLINE const FString& ToString() const
	{
		return ToStringReference().ToString();
	}

	FORCEINLINE FString GetLongPackageName() const
	{
		return ToStringReference().GetLongPackageName();
	}

	/**  
	 * Dereference lazy pointer to see if it points somewhere valid.
	 */
	FORCEINLINE explicit operator bool() const
	{
		return Get() != NULL;
	}

	/** Hash function. */
	FORCEINLINE friend uint32 GetTypeHash(const TAssetPtr<T>& Other)
	{
		return GetTypeHash(static_cast<const TPersistentObjectPtr<FStringAssetReference>&>(Other.AssetPtr));
	}

	friend FArchive& operator<<(FArchive& Ar, TAssetPtr<T>& Other)
	{
		Ar << Other.AssetPtr;
		return Ar;
	}

private:
	/** Wrapper of a UObject* */
	FAssetPtr AssetPtr;
};

template<class T> struct TIsPODType<TAssetPtr<T> > { enum { Value = TIsPODType<FAssetPtr>::Value }; };
template <class T> struct TIsWeakPointerType<TAssetPtr<T> > { enum { Value = TIsWeakPointerType<FAssetPtr>::Value }; };

template<class TClass>
class TAssetSubclassOf
{
	template <class U>
	friend class TAssetSubclassOf;

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
	FORCEINLINE TAssetSubclassOf(const TAssetSubclassOf<TClassA>& Other)
		: AssetPtr(Other.AssetPtr)
	{
		TClass* Test = (TClassA*)0; // verify that TClassA derives from TClass
	}

	/**  
	 * Construct from an class pointer
	 * @param From Class to create a reference to
	**/
	FORCEINLINE TAssetSubclassOf(const UClass* From)
		: AssetPtr(From)
	{
	}

	/**  
	 * Construct from a unique object identifier
	 * @param ObjectID Object identifier to create a weak pointer to
	**/
	explicit FORCEINLINE TAssetSubclassOf(const FStringAssetReference &ObjectID)
		: AssetPtr(ObjectID)
	{
	}

	/**
	 * Reset the lazy pointer back to the NULL state
	 */
	FORCEINLINE void Reset()
	{
		AssetPtr.Reset();
	}

	/**  
	 * Copy from an object pointer
	 * @param Object Object to create a lazy pointer to
	 */
	FORCEINLINE void operator=(const UClass* From)
	{
		AssetPtr = From;
	}

	/**  
	 * Copy from a unique object identifier
	 * @param ObjectID Object identifier to create a weak pointer to
	 */
	FORCEINLINE void operator=(const FStringAssetReference &ObjectID)
	{
		AssetPtr = ObjectID;
	}

	/**  
	 * Copy from a weak pointer
	 * @param Other Weak pointer to copy from
	 */
	template<class TClassA>
	FORCEINLINE void operator=(const TWeakObjectPtr<TClassA> &Other)
	{
		UClass* Test = (TClassA*)0; //verify that TClassA derives from UClass
		AssetPtr = Other;
	}

	/**  
	 * Construct from another lazy pointer
	 * @param Other lazy pointer to copy from
	 */
	template<class TClassA>
	FORCEINLINE void operator=(const TAssetSubclassOf<TClassA> &Other)
	{
		TClass* Test = (TClassA*)0; // verify that TClassA derives from TClass
		AssetPtr = Other;
	}

	/**  
	 * Compare lazy pointers for equality
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	 */
	FORCEINLINE friend bool operator==(const TAssetSubclassOf& Lhs, const TAssetSubclassOf& Rhs)
	{
		return Lhs.AssetPtr == Rhs.AssetPtr;
	}

	/**  
	 * Compare lazy pointers for inequality
	 * @param Other lazy pointer to compare to
	 * Caution: Two lazy pointers might not be equal to each other, but they both might return NULL.
	 */
	FORCEINLINE friend bool operator!=(const TAssetSubclassOf& Lhs, const TAssetSubclassOf& Rhs)
	{
		return Lhs.AssetPtr != Rhs.AssetPtr;
	}

	/**
	 * Gets the unique object identifier associated with this lazy pointer. Valid even if pointer is not currently valid
	 * @return Unique ID for this object, or an invalid FStringAssetReference if this pointer isn't set to anything
	 */
	FStringAssetReference GetUniqueID() const
	{
		return AssetPtr.GetUniqueID();
	}

	/**  
	 * Dereference the lazy pointer.
	 * @return NULL if this object is gone or the lazy pointer was NULL, otherwise a valid uobject pointer
	 */
	FORCEINLINE UClass *Get() const
	{
		UClass* Class = dynamic_cast<UClass*>(AssetPtr.Get());
		if (!Class || !Class->IsChildOf(TClass::StaticClass()))
		{
			return NULL;
		}
		return Class;
	}

	/**  
	 * Dereference the lazy pointer.
	 */
	FORCEINLINE UClass& operator*() const
	{
		return *Get();
	}

	/**  
	 * Dereference the lazy pointer.
	 */
	FORCEINLINE UClass* operator->() const
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
		return AssetPtr.IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return AssetPtr.IsNull();
	}

	/**  
	 * Returns the StringAssetReference that is wrapped by this AssetPtr
	 */
	FORCEINLINE const FStringAssetReference& ToStringReference() const
	{
		return AssetPtr.GetUniqueID();
	}

	/**  
	 * Dereference lazy pointer to see if it points somewhere valid.
	 */
	FORCEINLINE explicit operator bool() const
	{
		return Get() != NULL;
	}

	/** Hash function. */
	FORCEINLINE friend uint32 GetTypeHash(const TAssetSubclassOf<TClass>& Other)
	{
		return GetTypeHash(static_cast<const TPersistentObjectPtr<FStringAssetReference>&>(Other.AssetPtr));
	}

	/**  
	 * Synchronously load (if necessary) and return the asset object represented by this asset ptr
	 */
	UClass* LoadSynchronous()
	{
		UObject* Asset = AssetPtr.Get();
		if (Asset == nullptr && IsPending())
		{
			Asset = AssetPtr.LoadSynchronous();
		}
		UClass* Class = dynamic_cast<UClass*>(Asset);
		if (!Class || !Class->IsChildOf(TClass::StaticClass()))
		{
			return nullptr;
		}
		return Class;
	}

	friend FArchive& operator<<(FArchive& Ar, TAssetSubclassOf<TClass>& Other)
	{
		Ar << static_cast<FAssetPtr&>(Other.AssetPtr);
		return Ar;
	}

private:
	/** Wrapper of a UObject* */
	FAssetPtr AssetPtr;
};

template<class T> struct TIsPODType<TAssetSubclassOf<T> > { enum { Value = TIsPODType<FAssetPtr>::Value }; };
template <class T> struct TIsWeakPointerType<TAssetSubclassOf<T> > { enum { Value = TIsWeakPointerType<FAssetPtr>::Value }; };
