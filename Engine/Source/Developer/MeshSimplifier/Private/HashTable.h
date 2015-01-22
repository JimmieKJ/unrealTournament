// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/*-----------------------------------------------------------------------------
	Dynamically sized hash table, used to index another data structure.
	Vastly simpler and faster than TMap.

	Example find:

	uint32 Key = HashFunction( ID );
	for( uint32 i = HashTable.First( Key ); HashTable.IsValid( i ); i = HashTable.Next( i ) )
	{
		if( Array[i].ID == ID )
		{
			return Array[i];
		}
	}
-----------------------------------------------------------------------------*/
class FHashTable
{
public:
					FHashTable( uint16 InHashSize = 1024, uint32 InIndexSize = 0 );
					~FHashTable();

	void			Clear();
	void			Free();
	void			Resize( uint32 NewIndexSize );

	// Functions used to search
	uint32			First( uint16 Key ) const;
	uint32			Next( uint32 Index ) const;
	bool			IsValid( uint32 Index ) const;
	
	void			Add( uint16 Key, uint32 Index );
	void			Remove( uint16 Key, uint32 Index );

protected:
	// Avoids allocating hash until first add
	static uint32	EmptyHash[1];

	uint16			HashSize;
	uint16			HashMask;
	uint32			IndexSize;

	uint32*			Hash;
	uint32*			NextIndex;
};


FORCEINLINE FHashTable::FHashTable( uint16 InHashSize, uint32 InIndexSize )
	: HashSize( InHashSize )
	, HashMask( 0 )
	, IndexSize( InIndexSize )
	, Hash( EmptyHash )
	, NextIndex( NULL )
{
	check( FMath::IsPowerOfTwo( HashSize ) );
	
	if( IndexSize )
	{
		HashMask = HashSize - 1;
		
		Hash = new uint32[ HashSize ];
		NextIndex = new uint32[ IndexSize ];

		FMemory::Memset( Hash, 0xff, HashSize * 4 );
	}
}

FORCEINLINE FHashTable::~FHashTable()
{
	Free();
}

FORCEINLINE void FHashTable::Clear()
{
	if( IndexSize )
	{
		FMemory::Memset( Hash, 0xff, HashSize * 4 );
	}
}

FORCEINLINE void FHashTable::Free()
{
	if( IndexSize )
	{
		HashMask = 0;
		IndexSize = 0;
		
		delete[] Hash;
		Hash = EmptyHash;
		
		delete[] NextIndex;
		NextIndex = NULL;
	}
} 

// First in hash chain
FORCEINLINE uint32 FHashTable::First( uint16 Key ) const
{
	Key &= HashMask;
	return Hash[ Key ];
}

// Next in hash chain
FORCEINLINE uint32 FHashTable::Next( uint32 Index ) const
{
	checkSlow( Index < IndexSize );
	return NextIndex[ Index ];
}

FORCEINLINE bool FHashTable::IsValid( uint32 Index ) const
{
	return Index != ~0u;
}

FORCEINLINE void FHashTable::Add( uint16 Key, uint32 Index )
{
	if( Index >= IndexSize )
	{
		Resize( FMath::Max( 32u, FMath::RoundUpToPowerOfTwo( Index + 1 ) ) );
	}

	Key &= HashMask;
	NextIndex[ Index ] = Hash[ Key ];
	Hash[ Key ] = Index;
}

inline void FHashTable::Remove( uint16 Key, uint32 Index )
{
	if( Index >= IndexSize )
	{
		return;
	}

	Key &= HashMask;

	if( Hash[Key] == Index )
	{
		// Head of chain
		Hash[Key] = NextIndex[ Index ];
	}
	else
	{
		for( uint32 i = Hash[Key]; IsValid(i); i = NextIndex[i] )
		{
			if( NextIndex[i] == Index )
			{
				// Next = Next->Next
				NextIndex[i] = NextIndex[ Index ];
				break;
			}
		}
	}
}