// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"


/* FKeyHandleMap interface
 *****************************************************************************/

void FKeyHandleMap::Add( const FKeyHandle& InHandle, int32 InIndex )
{
	KeyHandlesToIndices.Add( InHandle, InIndex );
}


void FKeyHandleMap::Empty()
{
	KeyHandlesToIndices.Empty();
}


void FKeyHandleMap::Remove( const FKeyHandle& InHandle )
{
	KeyHandlesToIndices.Remove( InHandle );
}


const int32* FKeyHandleMap::Find( const FKeyHandle& InHandle ) const
{
	return KeyHandlesToIndices.Find( InHandle );
}


const FKeyHandle* FKeyHandleMap::FindKey( int32 KeyIndex ) const
{
	return KeyHandlesToIndices.FindKey( KeyIndex );
}


int32 FKeyHandleMap::Num() const
{
	return KeyHandlesToIndices.Num();
}


TMap<FKeyHandle, int32>::TConstIterator FKeyHandleMap::CreateConstIterator() const
{
	return KeyHandlesToIndices.CreateConstIterator();
}


TMap<FKeyHandle, int32>::TIterator FKeyHandleMap::CreateIterator() 
{
	return KeyHandlesToIndices.CreateIterator();
}


bool FKeyHandleMap::Serialize(FArchive& Ar)
{
	// only allow this map to be saved to the transaction buffer
	if( Ar.IsTransacting() )
	{
		Ar << KeyHandlesToIndices;
	}

	return true;
}


bool FKeyHandleMap::operator==(const FKeyHandleMap& Other) const
{
	if (KeyHandlesToIndices.Num() != Other.KeyHandlesToIndices.Num())
	{
		return false;
	}

	for (TMap<FKeyHandle, int32>::TConstIterator It(KeyHandlesToIndices); It; ++It)
	{
		int32 const* OtherVal = Other.KeyHandlesToIndices.Find(It.Key());
		if (!OtherVal || *OtherVal != It.Value() )
		{
			return false;
		}
	}

	return true;
}


bool FKeyHandleMap::operator!=(const FKeyHandleMap& Other) const
{
	return !(*this==Other);
}
