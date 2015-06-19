// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectIterator.h: High level iterators for uobject
=============================================================================*/

#ifndef __UOBJECTITERATOR_H__
#define __UOBJECTITERATOR_H__


/**
 * Class for iterating through all objects, including class default objects, unreachable objects...all UObjects
 */
class FRawObjectIterator : public FUObjectArray::TIterator
{
public:
	/**
	 * Constructor
	 * @param	bOnlyGCedObjects	if true, skip all of the permanent objects
	 */
	FRawObjectIterator(bool bOnlyGCedObjects = false) :
		FUObjectArray::TIterator( GetUObjectArray(), bOnlyGCedObjects )
	{
	}
	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE UObject* operator*() const
	{
		// casting UObjectBase to UObject for clients
		return (UObject *)GetObject();
	}
	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE UObject* operator->() const
	{
		return (UObject *)GetObject();
	}
};



/**
 * Class for iterating through all objects, including class default objects.
 * Note that when Playing In Editor, this will find objects in the
 * editor as well as the PIE world, in an indeterminate order.
 */
class FObjectIterator : public FUObjectArray::TIterator
{
public:
	/**
	 * Constructor
	 *
	 * @param	InClass						return only object of the class or a subclass
	 * @param	bOnlyGCedObjects			if true, skip all of the permanent objects
	 * @param	AdditionalExclusionFlags	RF_* flags that should not be included in results
	 */
	FObjectIterator( UClass* InClass=UObject::StaticClass(), bool bOnlyGCedObjects = false, EObjectFlags AdditionalExclusionFlags = RF_NoFlags ) :
		FUObjectArray::TIterator( GetUObjectArray(), bOnlyGCedObjects ),
		Class( InClass ),
		ExclusionFlags(AdditionalExclusionFlags)
	{
		// We don't want to return any objects that are currently being background loaded unless we're using the object iterator during async loading.
		ExclusionFlags = EObjectFlags(ExclusionFlags | RF_Unreachable);
		if (!IsAsyncLoading())
		{
			ExclusionFlags = EObjectFlags(ExclusionFlags | RF_AsyncLoading);
		}
		check(Class);

		do
		{
			UObject *Object = **this;
			if (!(Object->HasAnyFlags(ExclusionFlags) || (Class != UObject::StaticClass() && !Object->IsA(Class))))
			{
				break;
			}
		} while(Advance());
	}

	/**
	 * Constructor
	 *
	 * @param	Begin	The iterator to get the end iterator of.
	 */
	FObjectIterator( FUObjectArray::TIterator::EEndTagType, const FObjectIterator& Begin ) :
		FUObjectArray::TIterator( FUObjectArray::TIterator::EndTag, Begin ),
		Class( Begin.Class ),
		ExclusionFlags(Begin.ExclusionFlags)
	{
	}

	/**
	 * Iterator advance
	 */
	void operator++()
	{
		//@warning: behavior is partially mirrored in UnObjGC.cpp. Make sure to adapt code there as well if you make changes below.
		// verify that the async loading exclusion flag still matches (i.e. we didn't start/stop async loading within the scope of the iterator)
		checkSlow(IsAsyncLoading() || (ExclusionFlags & RF_AsyncLoading));

		while(Advance())
		{
			UObject *Object = **this;
			if (!(Object->HasAnyFlags(ExclusionFlags) || (Class != UObject::StaticClass() && !Object->IsA(Class))))
			{
				break;
			}
		}
	}
	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE UObject* operator*() const
	{
		// casting UObjectBase to UObject for clients
		return (UObject *)GetObject();
	}
	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE UObject* operator->() const
	{
		return (UObject *)GetObject();
	}
private:
	/** Class to restrict results to */
	UClass* Class;
protected:
	/** Flags that returned objects must not have */
	EObjectFlags ExclusionFlags;
};

/**
 * Class for iterating through all objects which inherit from a
 * specified base class.  Does not include any class default objects.
 * Note that when Playing In Editor, this will find objects in the
 * editor as well as the PIE world, in an indeterminate order.
 */
template< class T > class TObjectIterator
{
public:
	enum EEndTagType
	{
		EndTag
	};

	/**
	 * Constructor
	 */
	explicit TObjectIterator(EObjectFlags AdditionalExclusionFlags = RF_ClassDefaultObject, bool bIncludeDerivedClasses = true)
		: Index(-1)
	{
		GetObjectsOfClass(T::StaticClass(), ObjectArray, bIncludeDerivedClasses, AdditionalExclusionFlags);
		Advance();
	}

	/**
	* Constructor
	*/
	TObjectIterator(EEndTagType, const TObjectIterator& Begin)
		: Index(Begin.ObjectArray.Num())
	{
	}

	/**
	 * Iterator advance
	 */
	FORCEINLINE void operator++()
	{
		Advance();
	}

	/** Conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
	{ 
		return ObjectArray.IsValidIndex(Index); 
	}
	/** Conversion to "bool" returning true if the iterator is valid. */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE T* operator* () const
	{
		return (T*)GetObject();
	}
	/**
	 * Iterator dereference
	 * @return	the object pointer pointed at by the iterator
	 */
	FORCEINLINE T* operator-> () const
	{
		return (T*)GetObject();
	}

	FORCEINLINE friend bool operator==(const TObjectIterator& Lhs, const TObjectIterator& Rhs) { return Lhs.Index == Rhs.Index; }
	FORCEINLINE friend bool operator!=(const TObjectIterator& Lhs, const TObjectIterator& Rhs) { return Lhs.Index != Rhs.Index; }

protected:
	/**
	 * Dereferences the iterator with an ordinary name for clarity in derived classes
	 *
	 * @return	the UObject at the iterator
	 */
	FORCEINLINE UObject* GetObject() const 
	{ 
		return ObjectArray[Index];
	}
	
	/**
	 * Iterator advance with ordinary name for clarity in subclasses
	 * @return	true if the iterator points to a valid object, false if iteration is complete
	 */
	FORCEINLINE bool Advance()
	{
		//@todo UE4 check this for LHS on Index on consoles
		while(++Index < ObjectArray.Num())
		{
			if (GetObject())
			{
				return true;
			}
		}
		return false;
	}

protected:
	/** Results from the GetObjectsOfClass query */
	TArray<UObject*> ObjectArray;
	/** index of the current element in the object array */
	int32 Index;
};

/** specialization for T == UObject that does not call IsA() unnecessarily */
template<> class TObjectIterator<UObject> : public FObjectIterator
{
public:
	explicit TObjectIterator(EObjectFlags AdditionalExclusionFlags = RF_ClassDefaultObject) :
		FObjectIterator(UObject::StaticClass(), false, AdditionalExclusionFlags)
	{
	}

	/**
	 * Constructor
	 *
	 * @param	bOnlyGCedObjects			if true, skip all of the permanent objects
	 */
	explicit TObjectIterator(bool bOnlyGCedObjects) :
		FObjectIterator(UObject::StaticClass(), bOnlyGCedObjects, RF_ClassDefaultObject)
	{
	}

	/**
	 * Constructor
	 *
	 * @param	Begin	The iterator to get the end iterator of.
	 */
	TObjectIterator(FObjectIterator::EEndTagType, const TObjectIterator& Begin) :
		FObjectIterator( FObjectIterator::EndTag, Begin )
	{
	}

	/**
	 * Iterator advance
	 */
	void operator++()
	{
		// verify that the async loading exclusion flag still matches (i.e. we didn't start/stop async loading within the scope of the iterator)
		checkSlow(IsAsyncLoading() || (ExclusionFlags & RF_AsyncLoading));
		while(Advance())
		{
			if (!(*this)->HasAnyFlags(ExclusionFlags))
			{
				break;
			}
		}
	}
};

template <typename T>
struct TObjectRange
{
	TObjectRange(EObjectFlags AdditionalExclusionFlags = RF_ClassDefaultObject, bool bIncludeDerivedClasses = true)
		: Begin(AdditionalExclusionFlags, bIncludeDerivedClasses)
	{
	}

	friend TObjectIterator<T> begin(const TObjectRange& Range) { return Range.Begin; }
	friend TObjectIterator<T> end  (const TObjectRange& Range) { return TObjectIterator<T>(TObjectIterator<T>::EndTag, Range.Begin); }

	TObjectIterator<T> Begin;
};

template <>
struct TObjectRange<UObject>
{
	explicit TObjectRange(EObjectFlags AdditionalExclusionFlags = RF_ClassDefaultObject)
		: Begin(AdditionalExclusionFlags)
	{
	}

	explicit TObjectRange(bool bOnlyGCedObjects)
		: Begin(bOnlyGCedObjects)
	{
	}

	friend TObjectIterator<UObject> begin(const TObjectRange& Range) { return Range.Begin; }
	friend TObjectIterator<UObject> end  (const TObjectRange& Range) { return TObjectIterator<UObject>(TObjectIterator<UObject>::EndTag, Range.Begin); }

	TObjectIterator<UObject> Begin;
};


#endif	// __UOBJECTITERATOR_H__
