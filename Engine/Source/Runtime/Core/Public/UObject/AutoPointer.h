// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


template<class T, class TBASE>
class TAutoPointer : private TBASE
{
public:
	/** NULL constructor **/
	FORCEINLINE TAutoPointer()
	{
	}
	/** Construct from a raw pointer **/
	FORCEINLINE TAutoPointer(const T *Target) 
		: TBASE(Target)
	{
	}
	/**  Construct from the base type **/
	FORCEINLINE explicit TAutoPointer(const TBASE &Other) 
		: TBASE(Other)
	{
	}
	/**  Construct from another auto pointer **/
	template<class U, class UBASE>
	FORCEINLINE explicit TAutoPointer(const TAutoPointer<U,UBASE> &Other) 
		: TBASE(Other.Get())
	{
	}
	/** Reset the pointer back to the NULL state **/
	FORCEINLINE void Reset()
	{
		TBASE::Reset();
	}
	/**  Copy from a raw pointer **/
	template<class U>
	FORCEINLINE void operator=(const U *Target)
	{
		TBASE::operator=(Target);
	}
	/**  Construct from another base pointer type **/
	FORCEINLINE void operator=(const TBASE &Other)
	{
		TBASE::operator=(Other);
	}
	/**  Construct from another auto pointer **/
	template<class U, class UBASE>
	FORCEINLINE void operator=(const TAutoPointer<U,UBASE> &Other)
	{
		(*this) = Other.Get();
	}
	/**  Compare for equality with a base pointer **/
	FORCEINLINE bool operator==(const TBASE &Other) const
	{
		return (const TBASE&)*this == Other;
	}
	/**  Compare for equality with an auto pointer **/
	template<class U, class UBASE>
	FORCEINLINE bool operator==(const TAutoPointer<U,UBASE> &Other) const
	{
		return (const TBASE&)*this == Other;
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
	/**  Compare for inequality with a base pointer **/
	FORCEINLINE bool operator!=(const TBASE &Other) const
	{
		return (const TBASE&)*this != Other;
	}
	/**  Compare for equality with an auto pointer **/
	template<class U, class UBASE>
	FORCEINLINE bool operator!=(const TAutoPointer<U,UBASE> &Other) const
	{
		return (const TBASE&)*this != Other;
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
	/** Dereference **/
	FORCEINLINE T & operator*() const
	{
		return *Get();
	}
	/** Dereference **/
	FORCEINLINE T * operator->() const
	{
		return Get();
	}
	/** Dereference **/
	FORCEINLINE operator T* () const
	{
		return Get();
	}
	/** Conversion operator **/
	FORCEINLINE operator const T* () const
	{
		return (const T*)Get();
	}
	/**  NULL test **/
	FORCEINLINE operator bool() const
	{
		return !!IsValid();
	}
	/**  Dereference	**/
	FORCEINLINE T *Get() const
	{
		return TBASE::Get();
	}

	/**  Test if this pointer is NULL	**/
	FORCEINLINE bool IsValid() const
	{
		return TBASE::IsValid();
	}

	/** Hash function. */
	FORCEINLINE friend uint32 GetTypeHash(const TAutoPointer& WeakObjectPtr)
	{
		return GetTypeHash(static_cast<const TBASE&>(WeakObjectPtr));
	}

	friend FArchive& operator<<( FArchive& Ar, TAutoPointer& WeakObjectPtr )
	{
		Ar << static_cast<TBASE&>(WeakObjectPtr);
		return Ar;
	}

};


template<class T, class TBASE> struct TIsPODType<TAutoPointer<T, TBASE> > { enum { Value = TIsPODType<TBASE>::Value }; }; // pod-ness is the same as the podness of the base pointer type
