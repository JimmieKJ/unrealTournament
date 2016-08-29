// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Class.h"

/**
* Template to allow UClass's to be passed around with type safety
**/
template<class TClass>
class TSubclassOf
{
public:
	/**
	* Default Constructor, defaults to NULL
	**/
	FORCEINLINE TSubclassOf() :
		Class(NULL)
	{
	}
	/**
	* Constructor that takes a UClass and does a runtime check to make sure this is a compatible class
	* @param From UClass to assign to this TSubclassOf
	**/
	FORCEINLINE TSubclassOf(UClass* From) :
		Class(From)
	{
	}
	/**
	* Copy Constructor
	* @param From TSubclassOf to copy
	**/
	template<class TClassA>
	FORCEINLINE TSubclassOf(const TSubclassOf<TClassA>& From) :
		Class(*From)
	{
		TClass* Test = (TClassA*)0; // verify that TClassA derives from TClass
	}

	/**
	* Assignment operator
	* @param From TSubclassOf to copy
	**/
	template<class TClassA>
	FORCEINLINE TSubclassOf& operator=(const TSubclassOf<TClassA>& From)
	{
		Class = *From;
		TClass* Test = (TClassA*)0; // verify that TClassA derives from TClass
		return *this;
	}
	/**
	* Assignment operator from UClass, runtime check for compatibility
	* @param From UClass to assign to this TSubclassOf
	**/
	FORCEINLINE TSubclassOf& operator=(UClass* From)
	{
		Class = From;
		return *this;
	}
	/**
	* Dereference back into a UClass
	* @return	the embedded UClass
	* The body for this method is in Class.h
	*/
	FORCEINLINE UClass* operator*() const;

	/**
	* Dereference back into a UClass
	* @return	the embedded UClass
	*/
	FORCEINLINE UClass* operator->() const
	{
		return **this;
	}

	/**
	* Implicit conversion to UClass
	* @return	the embedded UClass
	*/
	FORCEINLINE operator UClass* () const
	{
		return **this;
	}

	/**
	* Get the CDO if we are referencing a valid class
	* @return the CDO, or NULL
	*/
	FORCEINLINE TClass* GetDefaultObject() const;

	friend FArchive& operator<<(FArchive& Ar, TSubclassOf& SubclassOf)
	{
		Ar << SubclassOf.Class;
		return Ar;
	}
private:
	UClass* Class;
};

/**
* Dereference back into a UClass
* @return	the embedded UClass
*/
template<class TClass>
FORCEINLINE UClass* TSubclassOf<TClass>::operator*() const
{
	if (!Class || !Class->IsChildOf(TClass::StaticClass()))
	{
		return NULL;
	}
	return Class;
}

template<class TClass>
FORCEINLINE TClass* TSubclassOf<TClass>::GetDefaultObject() const
{
	return Class ? Class->GetDefaultObject<TClass>() : NULL;
}
