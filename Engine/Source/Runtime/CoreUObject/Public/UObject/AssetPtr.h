// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AssetPtr.h: Pointer to UObject asset, keeps extra information so that it is works even if the asset is not in memory
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/Casts.h"
#include "UObject/PersistentObjectPtr.h"
#include "Misc/StringAssetReference.h"

/**
 * FAssetPtr is a type of weak pointer to a UObject, that also keeps track of the path to the object on disk.
 * It will change back and forth between being Valid and Pending as the referenced object loads or unloads.
 * It has no impact on if the object is garbage collected or not.
 *
 * This is useful to specify assets that you may want to asynchronously load on demand.
 */
class FAssetPtr : public TPersistentObjectPtr<FStringAssetReference>
{
public:	
	/** Default constructor, will be null */
	FORCEINLINE FAssetPtr()
	{
	}

	/** Construct from another asset pointer */
	FORCEINLINE FAssetPtr(const FAssetPtr& Other)
	{
		(*this)=Other;
	}

	/** Construct from a string asset reference */
	explicit FORCEINLINE FAssetPtr(const FStringAssetReference& AssetRef)
		: TPersistentObjectPtr<FStringAssetReference>(AssetRef)
	{
	}

	/** Construct from an object already in memory */
	explicit FORCEINLINE FAssetPtr(const UObject* Object)
	{
		(*this)=Object;
	}

	/** Synchronously load (if necessary) and return the asset object represented by this asset ptr */
	UObject* LoadSynchronous()
	{
		UObject* Asset = Get();
		if (Asset == nullptr && IsPending())
		{
			Asset = ToStringReference().TryLoad();
			*this = Asset;
		}
		return Asset;
	}

	/** Returns the StringAssetReference that is wrapped by this AssetPtr */
	FORCEINLINE const FStringAssetReference& ToStringReference() const
	{
		return GetUniqueID();
	}

	/** Returns string representation of reference, in form /package/path.assetname */
	FORCEINLINE const FString& ToString() const
	{
		return ToStringReference().ToString();
	}

	/** Returns /package/path string, leaving off the asset name */
	FORCEINLINE FString GetLongPackageName() const
	{
		return ToStringReference().GetLongPackageName();
	}
	
	/** Returns assetname string, leaving off the /package/path. part */
	FORCEINLINE FString GetAssetName() const
	{
		return ToStringReference().GetAssetName();
	}

	using TPersistentObjectPtr<FStringAssetReference>::operator=;
};

template <> struct TIsPODType<FAssetPtr> { enum { Value = TIsPODType<TPersistentObjectPtr<FStringAssetReference> >::Value }; };
template <> struct TIsWeakPointerType<FAssetPtr> { enum { Value = TIsWeakPointerType<TPersistentObjectPtr<FStringAssetReference> >::Value }; };

/**
 * TAssetPtr is templatized wrapper of the generic FAssetPtr, it can be used in UProperties
 */
template<class T=UObject>
class TAssetPtr
{
	template <class U>
	friend class TAssetPtr;

public:
	/** Default constructor, will be null */
	FORCEINLINE TAssetPtr()
	{
	}
	
	/** Construct from another asset pointer */
	template <class U, class = typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type>
	FORCEINLINE TAssetPtr(const TAssetPtr<U>& Other)
		: AssetPtr(Other.AssetPtr)
	{
	}

	/** Construct from an object already in memory */
	FORCEINLINE TAssetPtr(const T* Object)
		: AssetPtr(Object)
	{
	}

	/** Construct from a string asset reference */
	explicit FORCEINLINE TAssetPtr(const FStringAssetReference& AssetRef)
		: AssetPtr(AssetRef)
	{
	}

	/** Reset the asset pointer back to the null state */
	FORCEINLINE void Reset()
	{
		AssetPtr.Reset();
	}

	/** Copy from an object already in memory */
	FORCEINLINE TAssetPtr& operator=(const T* Object)
	{
		AssetPtr = Object;
		return *this;
	}

	/** Copy from a string asset reference */
	FORCEINLINE TAssetPtr& operator=(const FStringAssetReference& AssetRef)
	{
		AssetPtr = AssetRef;
		return *this;
	}

	/** Copy from a weak pointer to an object already in memory */
	template <class U, class = typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type>
	FORCEINLINE TAssetPtr& operator=(const TWeakObjectPtr<U>& Other)
	{
		AssetPtr = Other;
		return *this;
	}

	/** Copy from another asset pointer */
	template <class U, class = typename TEnableIf<TPointerIsConvertibleFromTo<U, T>::Value>::Type>
	FORCEINLINE TAssetPtr& operator=(const TAssetPtr<U>& Other)
	{
		AssetPtr = Other.AssetPtr;
		return *this;
	}

	/**  
	 * Compare asset pointers for equality
	 * Caution: Two asset pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other asset pointer to compare to 
	 */
	FORCEINLINE friend bool operator==(const TAssetPtr& Lhs, const TAssetPtr& Rhs)
	{
		return Lhs.AssetPtr == Rhs.AssetPtr;
	}

	/**  
	 * Compare asset pointers for inequality
	 * Caution: Two asset pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other asset pointer to compare to
	 */
	FORCEINLINE friend bool operator!=(const TAssetPtr& Lhs, const TAssetPtr& Rhs)
	{
		return Lhs.AssetPtr != Rhs.AssetPtr;
	}

	/**  
	 * Dereference the asset pointer.
	 *
	 * @return nullptr if this object is gone or the lazy pointer was null, otherwise a valid UObject pointer
	 */
	FORCEINLINE T* Get() const
	{
		return dynamic_cast<T*>(AssetPtr.Get());
	}

	/** Dereference the asset pointer */
	FORCEINLINE T& operator*() const
	{
		return *Get();
	}

	/** Dereference the asset pointer */
	FORCEINLINE T* operator->() const
	{
		return Get();
	}

	/** Synchronously load (if necessary) and return the asset object represented by this asset ptr */
	T* LoadSynchronous()
	{
		UObject* Asset = AssetPtr.LoadSynchronous();
		return Cast<T>(Asset);
	}

	/**  
	 * Test if this points to a live UObject
	 *
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid() const
	{
		// This does the runtime type check
		return Get() != nullptr;
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 *
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return AssetPtr.IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 *
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return AssetPtr.IsNull();
	}

	/** Returns the StringAssetReference that is wrapped by this AssetPtr */
	FORCEINLINE const FStringAssetReference& GetUniqueID() const
	{
		return AssetPtr.GetUniqueID();
	}

	/** Returns the StringAssetReference that is wrapped by this AssetPtr */
	FORCEINLINE const FStringAssetReference& ToStringReference() const
	{
		return AssetPtr.ToStringReference();
	}

	/** Returns string representation of reference, in form /package/path.assetname */
	FORCEINLINE const FString& ToString() const
	{
		return ToStringReference().ToString();
	}

	/** Returns /package/path string, leaving off the asset name */
	FORCEINLINE FString GetLongPackageName() const
	{
		return ToStringReference().GetLongPackageName();
	}
	
	/** Returns assetname string, leaving off the /package/path part */
	FORCEINLINE FString GetAssetName() const
	{
		return ToStringReference().GetAssetName();
	}

	/** Dereference asset pointer to see if it points somewhere valid */
	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	/** Hash function */
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
	FAssetPtr AssetPtr;
};

template<class T> struct TIsPODType<TAssetPtr<T> > { enum { Value = TIsPODType<FAssetPtr>::Value }; };
template<class T> struct TIsWeakPointerType<TAssetPtr<T> > { enum { Value = TIsWeakPointerType<FAssetPtr>::Value }; };

/**
 * TAssetSubclassOf is a templatized wrapper around FAssetPtr that works like a TSubclassOf, it can be used in UProperties for blueprint subclasses
 */
template<class TClass>
class TAssetSubclassOf
{
	template <class TClassA>
	friend class TAssetSubclassOf;

public:
	/** Default constructor, will be null */
	FORCEINLINE TAssetSubclassOf()
	{
	}
		
	/** Construct from another asset pointer */
	template <class TClassA, class = typename TEnableIf<TPointerIsConvertibleFromTo<TClassA, TClass>::Value>::Type>
	FORCEINLINE TAssetSubclassOf(const TAssetSubclassOf<TClassA>& Other)
		: AssetPtr(Other.AssetPtr)
	{
	}

	/** Construct from a class already in memory */
	FORCEINLINE TAssetSubclassOf(const UClass* From)
		: AssetPtr(From)
	{
	}

	/** Construct from a string asset reference */
	explicit FORCEINLINE TAssetSubclassOf(const FStringAssetReference& AssetRef)
		: AssetPtr(AssetRef)
	{
	}

	/** Reset the asset pointer back to the null state */
	FORCEINLINE void Reset()
	{
		AssetPtr.Reset();
	}

	/** Copy from a class already in memory */
	FORCEINLINE void operator=(const UClass* From)
	{
		AssetPtr = From;
	}

	/** Copy from a string asset reference */
	FORCEINLINE void operator=(const FStringAssetReference& AssetRef)
	{
		AssetPtr = AssetRef;
	}

	/** Copy from a weak pointer already in memory */
	template<class TClassA, class = typename TEnableIf<TPointerIsConvertibleFromTo<TClassA, UClass>::Value>::Type>
	FORCEINLINE TAssetSubclassOf& operator=(const TWeakObjectPtr<TClassA>& Other)
	{
		AssetPtr = Other;
		return *this;
	}

	/** Copy from another asset pointer */
	template<class TClassA, class = typename TEnableIf<TPointerIsConvertibleFromTo<TClassA, TClass>::Value>::Type>
	FORCEINLINE TAssetSubclassOf& operator=(const TAssetPtr<TClassA>& Other)
	{
		AssetPtr = Other.AssetPtr;
		return *this;
	}

	/**  
	 * Compare asset pointers for equality
	 * Caution: Two asset pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other asset pointer to compare to 
	 */
	FORCEINLINE friend bool operator==(const TAssetSubclassOf& Lhs, const TAssetSubclassOf& Rhs)
	{
		return Lhs.AssetPtr == Rhs.AssetPtr;
	}

	/**  
	 * Compare asset pointers for inequality
	 * Caution: Two asset pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other asset pointer to compare to
	 */
	FORCEINLINE friend bool operator!=(const TAssetSubclassOf& Lhs, const TAssetSubclassOf& Rhs)
	{
		return Lhs.AssetPtr != Rhs.AssetPtr;
	}

	/**  
	 * Dereference the asset pointer
	 *
	 * @return nullptr if this object is gone or the asset pointer was null, otherwise a valid UClass pointer
	 */
	FORCEINLINE UClass* Get() const
	{
		UClass* Class = dynamic_cast<UClass*>(AssetPtr.Get());
		if (!Class || !Class->IsChildOf(TClass::StaticClass()))
		{
			return nullptr;
		}
		return Class;
	}

	/** Dereference the asset pointer */
	FORCEINLINE UClass& operator*() const
	{
		return *Get();
	}

	/** Dereference the asset pointer */
	FORCEINLINE UClass* operator->() const
	{
		return Get();
	}

	/**  
	 * Test if this points to a live UObject
	 *
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid() const
	{
		// This also does the UClass type check
		return Get() != nullptr;
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 *
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return AssetPtr.IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 *
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return AssetPtr.IsNull();
	}

	/** Returns the StringAssetReference that is wrapped by this AssetPtr */
	FORCEINLINE const FStringAssetReference& GetUniqueID() const
	{
		return AssetPtr.GetUniqueID();
	}

	/** Returns the StringAssetReference that is wrapped by this AssetPtr */
	FORCEINLINE const FStringAssetReference& ToStringReference() const
	{
		return AssetPtr.GetUniqueID();
	}

	/** Returns string representation of reference, in form /package/path.assetname  */
	FORCEINLINE const FString& ToString() const
	{
		return ToStringReference().ToString();
	}

	/** Returns /package/path string, leaving off the asset name */
	FORCEINLINE FString GetLongPackageName() const
	{
		return ToStringReference().GetLongPackageName();
	}
	
	/** Returns assetname string, leaving off the /package/path part */
	FORCEINLINE FString GetAssetName() const
	{
		return ToStringReference().GetAssetName();
	}

	/** Dereference asset pointer to see if it points somewhere valid */
	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	/** Hash function */
	FORCEINLINE friend uint32 GetTypeHash(const TAssetSubclassOf<TClass>& Other)
	{
		return GetTypeHash(static_cast<const TPersistentObjectPtr<FStringAssetReference>&>(Other.AssetPtr));
	}

	/** Synchronously load (if necessary) and return the asset object represented by this asset ptr */
	UClass* LoadSynchronous()
	{
		UObject* Asset = AssetPtr.LoadSynchronous();
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
	FAssetPtr AssetPtr;
};

template <class T> struct TIsPODType<TAssetSubclassOf<T> > { enum { Value = TIsPODType<FAssetPtr>::Value }; };
template <class T> struct TIsWeakPointerType<TAssetSubclassOf<T> > { enum { Value = TIsWeakPointerType<FAssetPtr>::Value }; };
