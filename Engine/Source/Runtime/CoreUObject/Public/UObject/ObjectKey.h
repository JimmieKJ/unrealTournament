// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WeakObjectPtrTemplates.h"
#include "WeakObjectPtr.h"

/** FObjectKey is an immutable, copyable key which can be used to uniquely identify an object for the lifetime of the application */
struct FObjectKey
{
public:
	/** Deafult constructor */
	FORCEINLINE FObjectKey()
		: ObjectIndex(INDEX_NONE)
		, ObjectSerialNumber(0)
	{
	}

	/** Construct from an object pointer */
	FORCEINLINE FObjectKey(const UObject *Object)
	    : ObjectIndex(INDEX_NONE)
	    , ObjectSerialNumber(0)
	{
	    if (Object)
	    {
	        FWeakObjectPtr Weak(Object);
	        ObjectIndex = Weak.ObjectIndex;
	        ObjectSerialNumber = Weak.ObjectSerialNumber;
	    }
	}

	/** Compare this key with another */
	FORCEINLINE bool operator==(const FObjectKey &Other) const
	{
		return ObjectIndex == Other.ObjectIndex && ObjectSerialNumber == Other.ObjectSerialNumber;
	}

	/** Compare this key with another */
	FORCEINLINE bool operator!=(const FObjectKey &Other) const
	{
		return ObjectIndex != Other.ObjectIndex || ObjectSerialNumber != Other.ObjectSerialNumber;
	}

	/** Hash function */
	friend uint32 GetTypeHash(const FObjectKey& Key)
	{
		return HashCombine(Key.ObjectIndex, Key.ObjectSerialNumber);
	}

private:

	int32		ObjectIndex;
	int32		ObjectSerialNumber;
};
