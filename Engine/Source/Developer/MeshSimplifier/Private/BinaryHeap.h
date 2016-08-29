// Copyright (C) 2009 Nine Realms, Inc
//

#pragma once

/*-----------------------------------------------------------------------------
	Binary Heap, used to index another data structure.

	Also known as a priority queue. Smallest key at top.
	KeyType must implement operator<
-----------------------------------------------------------------------------*/
template< typename KeyType >
class FBinaryHeap
{
public:
				FBinaryHeap();
				FBinaryHeap( uint32 InHeapSize, uint32 InIndexSize );
				~FBinaryHeap();

	void		Clear();
	void		Free();
	void		Resize( uint32 NewHeapSize, uint32 NewIndexSize );

	uint32		Num() const				{ return HeapNum; }
	uint32		GetHeapSize() const		{ return HeapSize; }
	uint32		GetIndexSize() const	{ return IndexSize; }

	bool		IsPresent( uint32 Index ) const;
	KeyType		GetKey( uint32 Index ) const;

	uint32		Top() const;	
	void		Pop();

	void		Add( KeyType Key, uint32 Index );
	void		Update( KeyType Key, uint32 Index );
	void		Remove( uint32 Index );

protected:
	void		ResizeHeap( uint32 NewHeapSize );
	void		ResizeIndexes( uint32 NewIndexSize );

	void		UpHeap( uint32 HeapIndex );
	void		DownHeap( uint32 HeapIndex );
	void		Verify();

	uint32		HeapNum;
	uint32		HeapSize;
	uint32		IndexSize;
	
	uint32*		Heap;

	KeyType*	Keys;
	uint32*		HeapIndexes;
};

template< typename KeyType >
FORCEINLINE FBinaryHeap< KeyType >::FBinaryHeap()
	: HeapNum(0)
	, HeapSize(0)
	, IndexSize(0)
	, Heap( NULL )
	, Keys( NULL )
	, HeapIndexes( NULL )
{}

template< typename KeyType >
FORCEINLINE FBinaryHeap< KeyType >::FBinaryHeap( uint32 InHeapSize, uint32 InIndexSize )
	: HeapNum(0)
	, HeapSize( InHeapSize )
	, IndexSize( InIndexSize )
{
	Heap = new uint32[ HeapSize ];
	Keys = new KeyType[ IndexSize ];
	HeapIndexes = new uint32[ IndexSize ];

	FMemory::Memset( HeapIndexes, 0xff, IndexSize * 4 );
}

template< typename KeyType >
FORCEINLINE FBinaryHeap< KeyType >::~FBinaryHeap()
{
	Free();
}

template< typename KeyType >
FORCEINLINE void FBinaryHeap< KeyType >::Clear()
{
	HeapNum = 0;
	FMemory::Memset( HeapIndexes, 0xff, IndexSize * 4 );
}

template< typename KeyType >
FORCEINLINE void FBinaryHeap< KeyType >::Free()
{
	HeapNum = 0;
	HeapSize = 0;
	IndexSize = 0;
	
	delete[] Heap;
	delete[] Keys;
	delete[] HeapIndexes;

	Heap = NULL;
	Keys = NULL;
	HeapIndexes = NULL;
}

template< typename KeyType >
void FBinaryHeap< KeyType >::ResizeHeap( uint32 NewHeapSize )
{
	checkSlow( NewHeapSize != HeapSize );

	if( NewHeapSize == 0 )
	{
		HeapNum = 0;
		HeapSize = 0;
		
		delete[] Heap;
		Heap = NULL;

		return;
	}

	uint32* NewHeap = new uint32[ NewHeapSize ];

	if( HeapSize != 0 )
	{
		FMemory::Memcpy( NewHeap, Heap, HeapSize * 4 );
		delete[] Heap;
	}
	
	HeapNum		= FMath::Min( HeapNum, NewHeapSize );
	HeapSize	= NewHeapSize;
	Heap		= NewHeap;
}

template< typename KeyType >
void FBinaryHeap< KeyType >::ResizeIndexes( uint32 NewIndexSize )
{
	checkSlow( NewIndexSize != IndexSize );

	if( NewIndexSize == 0 )
	{
		IndexSize = 0;
		
		delete[] Keys;
		delete[] HeapIndexes;

		Keys = NULL;
		HeapIndexes = NULL;

		return;
	}

	KeyType* NewKeys = new KeyType[ NewIndexSize ];
	uint32* NewHeapIndexes = new uint32[ NewIndexSize ];

	if( IndexSize != 0 )
	{
		check( NewIndexSize >= IndexSize );

		for( uint32 i = 0; i < IndexSize; i++ )
		{
			NewKeys[i] = Keys[i];
			NewHeapIndexes[i] = HeapIndexes[i];
		}
		for( uint32 i = IndexSize; i < NewIndexSize; i++ )
		{
			NewHeapIndexes[i] = ~0u;
		}
		delete[] Keys;
		delete[] HeapIndexes;
	}
	
	IndexSize	= NewIndexSize;
	Keys		= NewKeys;
	HeapIndexes	= NewHeapIndexes;
}

template< typename KeyType >
FORCEINLINE void FBinaryHeap< KeyType >::Resize( uint32 NewHeapSize, uint32 NewIndexSize )
{
	if( NewHeapSize == HeapSize )
	{
		ResizeHeap( NewHeapSize );
	}

	if( NewIndexSize != IndexSize )
	{
		ResizeIndexes( NewIndexSize );
	}
}

template< typename KeyType >
FORCEINLINE bool FBinaryHeap< KeyType >::IsPresent( uint32 Index ) const
{
	checkSlow( Index < IndexSize );
	return HeapIndexes[ Index ] != ~0u;
}

template< typename KeyType >
FORCEINLINE KeyType FBinaryHeap< KeyType >::GetKey( uint32 Index ) const
{
	checkSlow( IsPresent( Index ) );
	return Keys[ Index ];
}

template< typename KeyType >
FORCEINLINE uint32 FBinaryHeap< KeyType >::Top() const
{
	checkSlow( Heap );
	checkSlow( HeapNum > 0 );
	return Heap[0];
}

template< typename KeyType >
FORCEINLINE void FBinaryHeap< KeyType >::Pop()
{
	checkSlow( Heap );
	checkSlow( HeapNum > 0 );

	uint32 Index = Heap[0];
	
	Heap[0] = Heap[ --HeapNum ];
	HeapIndexes[ Heap[0] ] = 0;
	HeapIndexes[ Index ] = ~0u;

	DownHeap(0);
}

template< typename KeyType >
FORCEINLINE void FBinaryHeap< KeyType >::Add( KeyType Key, uint32 Index )
{
	if( HeapNum == HeapSize )
	{
		ResizeHeap( FMath::Max( 32u, HeapSize * 2 ) );
	}

	if( Index >= IndexSize )
	{
		ResizeIndexes( FMath::Max( 32u, FMath::RoundUpToPowerOfTwo( Index + 1 ) ) );
	}

	checkSlow( !IsPresent( Index ) );

	uint32 HeapIndex = HeapNum++;
	Heap[ HeapIndex ] = Index;
	
	Keys[ Index ] = Key;
	HeapIndexes[ Index ] = HeapIndex;

	UpHeap( HeapIndex );
}

template< typename KeyType >
FORCEINLINE void FBinaryHeap< KeyType >::Update( KeyType Key, uint32 Index )
{
	checkSlow( Heap );
	checkSlow( IsPresent( Index ) );

	Keys[ Index ] = Key;

	uint32 HeapIndex = HeapIndexes[ Index ];
	if( HeapIndex > 0 && Key < Keys[ Heap[(HeapIndex - 1) >> 1] ] )
	{
		UpHeap( HeapIndex );
	}
	else
	{
		DownHeap( HeapIndex );
	}
}

template< typename KeyType >
FORCEINLINE void FBinaryHeap< KeyType >::Remove( uint32 Index )
{
	checkSlow( Heap );
	
	if( !IsPresent( Index ) )
	{
		return;
	}

	KeyType Key = Keys[ Index ];
	uint32 HeapIndex = HeapIndexes[ Index ];

	Heap[ HeapIndex ] = Heap[ --HeapNum ];
	HeapIndexes[ Heap[ HeapIndex ] ] = Index;
	HeapIndexes[ Index ] = ~0u;

	if( Key < Keys[ Heap[ HeapIndex ] ] )
	{
		UpHeap( HeapIndex );
	}
	else
	{
		DownHeap( HeapIndex );
	}
}

template< typename KeyType >
void FBinaryHeap< KeyType >::UpHeap( uint32 HeapIndex )
{
	uint32 Moving = Heap[ HeapIndex ];
	uint32 i = HeapIndex;
	uint32 Parent = (i - 1) >> 1;

	while( i > 0 && Keys[ Moving ] < Keys[ Heap[ Parent ] ] )
	{
		Heap[i] = Heap[ Parent ];
		HeapIndexes[ Heap[i] ] = i;

		i = Parent;
		Parent = (i - 1) >> 1;
	}

	if( i != HeapIndex )
	{
		Heap[i] = Moving;
		HeapIndexes[ Heap[i] ] = i;
	}
}

template< typename KeyType >
void FBinaryHeap< KeyType >::DownHeap( uint32 HeapIndex )
{
	uint32 Moving = Heap[ HeapIndex ];
	uint32 i = HeapIndex;
	uint32 Left = (i << 1) + 1;
	uint32 Right = Left + 1;
	
	while( Left < HeapNum )
	{
		uint32 Smallest = Left;
		if( Right < HeapNum )
		{
			Smallest = ( Keys[ Heap[Left] ] < Keys[ Heap[Right] ] ) ? Left: Right;
		}

		if( Keys[ Heap[Smallest] ] < Keys[ Moving ] )
		{
			Heap[i] = Heap[ Smallest ];
			HeapIndexes[ Heap[i] ] = i;

			i = Smallest;
			Left = (i << 1) + 1;
			Right = Left + 1;
		}
		else
		{
			break;
		}
	}

	if( i != HeapIndex )
	{
		Heap[i] = Moving;
		HeapIndexes[ Heap[i] ] = i;
	}
}

template< typename KeyType >
void FBinaryHeap< KeyType >::Verify()
{
	for( uint32 i = 1; i < HeapNum; i++ )
	{
		uint32 Parent = (i - 1) >> 1;
		if( Keys[ Heap[i] ] < Keys[ Heap[Parent] ] )
		{
			check( false );
		}
	}
}