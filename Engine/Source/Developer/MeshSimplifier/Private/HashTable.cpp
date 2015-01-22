// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"
#include "HashTable.h"

uint32 FHashTable::EmptyHash[1] = { ~0u };

void FHashTable::Resize( uint32 NewIndexSize )
{
	if( NewIndexSize == IndexSize )
	{
		return;
	}

	if( NewIndexSize == 0 )
	{
		Free();
		return;
	}

	if( IndexSize == 0 )
	{
		HashMask = HashSize - 1;
		Hash = new uint32[ HashSize ];
		FMemory::Memset( Hash, 0xff, HashSize * 4 );
	}

	uint32* NewNextIndex = new uint32[ NewIndexSize ];

	if( NextIndex )
	{
		FMemory::Memcpy( NewNextIndex, NextIndex, IndexSize * 4 );
		delete[] NextIndex;
	}
	
	IndexSize = NewIndexSize;
	NextIndex = NewNextIndex;
}