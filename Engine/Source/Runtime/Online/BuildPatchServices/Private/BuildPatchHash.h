// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchHash.h: Declares and implements template classes for use with the 
	Build and Patching hash functionality.
=============================================================================*/

#ifndef __BuildPatchHash_h__
#define __BuildPatchHash_h__

#pragma once

/**
 * A macro for barrel rolling a 64 bit value n times to the left.
 * @param Value		The value to shift
 * @param Shifts	The number of times to shift
 */
#define ROTLEFT_64B( Value, Shifts ) Value = ( ( ( Value ) << ( ( Shifts ) % 64 ) ) | ( ( Value ) >> ( ( 64 - ( ( Shifts ) % 64 ) ) % 64 ) ) )

/**
 * A ring buffer template class.
 */
template< typename DataType, uint32 BufferDataSize >
class TRingBuffer
{
public:
	/**
	 * Default Constructor
	 */
	TRingBuffer();

	/**
	 * Default Destructor
	 */
	~TRingBuffer();

	/**
	 * Clears memory and indexes
	 */
	void Empty();

	/**
	 * Pushes a data word to the end of the FIFO. WILL OVERWRITE OLDEST if full.
	 * @param Val	The data word to push
	 */
	void Enqueue( const DataType& Val );

	/**
	 * Pushes a buffer of data words to the end of the FIFO. WILL OVERWRITE OLDEST if full.
	 * @param ValBuf		The buffer pointer
	 * @param BufLen		The length to copy
	 */
	void Enqueue( const DataType* ValBuf, const uint32& BufLen );

	/**
	 * Take the next data word from the FIFO buffer.
	 * @param OUT	ValOut	The variable to receive the data word
	 * @return	true if the buffer was not empty, false otherwise.
	 */
	bool Dequeue( DataType& ValOut );

	/**
	 * Take the next set of data words from the FIFO buffer.
	 * @param ValBuf		The buffer to receive the data
	 * @param BufLen		The number of words requested
	 * @return	the number of words actually copied
	 */
	uint32 Dequeue( DataType* ValBuf, const uint32& BufLen );

	/**
	 * Get the next data word from the FIFO buffer without removing it.
	 * @param OUT	ValOut	The variable to receive the data word
	 * @return	true if the buffer was not empty, false otherwise.
	 */
	bool Peek( DataType& ValOut ) const;

	/**
	 * Get the next set of data words from the FIFO buffer without removing them.
	 * @param ValBuf		The buffer to receive the data
	 * @param BufLen		The number of words requested
	 * @return	the number of words actually copied
	 */
	uint32 Peek( DataType* ValBuf, const uint32& BufLen ) const;

	/**
	 * Compare the memory in the FIFO to the memory in the given buffer
	 * @param SerialBuffer	The buffer containing data to compare
	 * @param CompareLen		The number of words to compare
	 * @return	< 0 if data < SerialBuffer.. 0 if data == SerialBuffer... > 0 if data > SerialBuffer
	 */
	int32 SerialCompare( const DataType* SerialBuffer, uint32 CompareLen ) const;

	/**
	 * Serializes the internal buffer into the given buffer
	 * @param SerialBuffer	A preallocated buffer to copy data into
	 */
	void Serialize( DataType* SerialBuffer ) const;

	/**
	 * Square bracket operators for accessing data in the buffer by FIFO index. [0] returns the next entry in the FIFO that would get provided by Dequeue or Peek.
	 * @param Index	The index into the FIFO buffer
	 * @return	The data word
	 */
	FORCEINLINE const DataType& operator[]( const int32& Index ) const;
	FORCEINLINE DataType& operator[]( const int32& Index );

	/**
	 * Gets the last data word in the FIFO (i.e. most recently pushed).
	 * @return	The last data word
	 */
	FORCEINLINE const DataType& Top() const;
	FORCEINLINE DataType& Top();

	/**
	 * Gets the first data word in the FIFO (i.e. oldest).
	 * @return	The first data word
	 */
	FORCEINLINE const DataType& Bottom() const;
	FORCEINLINE DataType& Bottom();

	/**
	 * Gets the buffer index that the last data word is stored in.
	 * @return	The index of Top()
	 */
	FORCEINLINE const uint32 TopIndex() const;

	/**
	 * Gets the buffer index that the first data word is stored in.
	 * @return	The index of Bottom()
	 */
	FORCEINLINE const uint32 BottomIndex() const;

	/**
	 * Gets the buffer index that the next enqueued word will get stored in.
	 * @return	The index that the next enqueued word will have
	 */
	FORCEINLINE const uint32 NextIndex() const;

	/**
	 * Gets the size of the data buffer.
	 * @return	Template arg BufferDataSize
	 */
	FORCEINLINE const uint32 RingDataSize() const;

	/**
	 * Gets the number of words currently in the FIFO.
	 * @return	data size
	 */
	FORCEINLINE const uint32 RingDataUsage() const;

	/**
	 * Gets the total number of words that have been pushed through this buffer since clearing
	 * @return	total length of data passed through
	 */
	FORCEINLINE const uint64 TotalDataPushed() const;


private:
	// The data memory
	DataType* Data;

	// Value keeping track of free space
	uint32 NumDataAvailable;

	// Value keeping track of the next data index
	uint32 DataIndex;

	// Value to keep track of total amount of data Enqueued
	uint64 TotalNumDataPushed;
};

/**
 * A static struct containing 64bit polynomial constant and hash table lookup for use with FRollingHash.
 */
struct FRollingHashConst
{
	// The lookup hash table
	static uint64 HashTable[ 256 ];
	
	/**
	 * Builds the hash table for use when hashing. Must be called before using FRollingHash.
	 */
	static void Init();
};

/**
 * A class that performs a rolling hash
 */
template< uint32 WindowSize >
class FRollingHash
{
	// A typedef for our data ring buffer
	typedef TRingBuffer< uint8, WindowSize > HashRingBuffer;

public:
	/**
	 * Constructor
	 */
	FRollingHash();

	/**
	 * Pass this function the initial data set to start the Rolling Hash with.
	 * @param NewByte		The byte to add to the hash state
	 */
	void ConsumeByte( const uint8& NewByte );

	/**
	 * Helper to consume from a byte array
	 * @param NewBytes		The byte array
	 * @param NumBytes		The number of bytes to consume
	 */
	void ConsumeBytes( const uint8* NewBytes, const uint32& NumBytes );

	/**
	 * Rolls the window by one byte forwards.
	 * @param NewByte		The byte that will be added to the front of the window
	 */
	void RollForward( const uint8& NewByte );

	/**
	 * Clears all data ready for a new entire data set
	 */
	void Clear();

	/**
	 * Get the HashState for the current window
	 * @return		The hash state
	 */
	const uint64 GetWindowHash() const;

	/**
	 * Get the Ring Buffer for the current window
	 * @return		The ring buffer
	 */
	const HashRingBuffer& GetWindowData() const;

	/**
	 * Get how many DataValueType values we still need to consume until our window is full 
	 * @return		The number of DataValueType we still need
	 */
	const uint32 GetNumDataNeeded() const;

	/**
	 * Get the size of our window 
	 * @return		Template arg WindowSize
	 */
	const uint32 GetWindowSize() const;

	/**
	 * Static function to simply return the hash for a given data range.
	 * @param DataSet	The buffer to the data. This must be a buffer of length == WindowSize
	 * @return			The hash state for the provided data
	 */
	static uint64 GetHashForDataSet( const uint8* DataSet );

private:
	// The current hash value
	uint64 HashState;
	// The number of bytes we have consumed so far, used in hash function and to check validity of calls
	uint32 NumBytesConsumed;
	// Store the data to make access and rolling easier
	HashRingBuffer WindowData;
};

namespace FCycPoly64Hash
{
	/**
	 * Calculate the hash for a given data range.
	 * @param DataSet	The buffer to the data. This must be a buffer of length == DataSize
	 * @param DataSize	The size of the buffer
	 * @param State		The current hash state, if continuing hash from previous buffer. 0 for new data.
	 * @return			The hash state for the provided data
	 */
	uint64 GetHashForDataSet(const uint8* DataSet, const uint32 DataSize, const uint64 State = 0);
}

/* TRingBuffer implementation
*****************************************************************************/
template< typename DataType, uint32 BufferDataSize >
TRingBuffer< DataType, BufferDataSize >::TRingBuffer()
{
	Data = new DataType[ BufferDataSize ];
	Empty();
}

template< typename DataType, uint32 BufferDataSize >
TRingBuffer< DataType, BufferDataSize >::~TRingBuffer()
{
	delete[] Data;
}

template< typename DataType, uint32 BufferDataSize >
void TRingBuffer< DataType, BufferDataSize >::Empty()
{
	FMemory::Memzero( Data, sizeof( DataType ) * BufferDataSize );
	NumDataAvailable = 0;
	DataIndex = 0;
	TotalNumDataPushed = 0;
}

template< typename DataType, uint32 BufferDataSize >
void TRingBuffer< DataType, BufferDataSize >::Enqueue( const DataType& Val )
{
	Data[ DataIndex++ ] = Val;
	DataIndex %= BufferDataSize;
	++TotalNumDataPushed;
	NumDataAvailable = FMath::Clamp< uint32 >( NumDataAvailable + 1, 0, BufferDataSize );
}

template< typename DataType, uint32 BufferDataSize >
void TRingBuffer< DataType, BufferDataSize >::Enqueue( const DataType* ValBuf, const uint32& BufLen )
{
	for( uint32 i = 0; i < BufLen; ++i )
	{
		Enqueue( ValBuf[i] );
	}
}

template< typename DataType, uint32 BufferDataSize >
bool TRingBuffer< DataType, BufferDataSize >::Dequeue( DataType& ValOut )
{
	if( NumDataAvailable > 0 )
	{
		ValOut = Bottom();
		NumDataAvailable = FMath::Clamp< uint32 >( NumDataAvailable - 1, 0, BufferDataSize );
		return true;
	}
	else
	{
		return false;
	}
}

template< typename DataType, uint32 BufferDataSize >
uint32 TRingBuffer< DataType, BufferDataSize >::Dequeue( DataType* ValBuf, const uint32& BufLen )
{
	uint32 Count = 0;
	for( uint32 i = 0; i < BufLen; ++i )
	{
		if( Dequeue( ValBuf[i] ) )
		{
			++Count;
		}
		else
		{
			break;
		}
	}
	return Count;
}

template< typename DataType, uint32 BufferDataSize >
bool TRingBuffer< DataType, BufferDataSize >::Peek( DataType& ValOut ) const
{
	if( NumDataAvailable > 0 )
	{
		ValOut = Bottom();
		return true;
	}
	else
	{
		return false;
	}
}

template< typename DataType, uint32 BufferDataSize >
uint32 TRingBuffer< DataType, BufferDataSize >::Peek( DataType* ValBuf, const uint32& BufLen ) const
{
	uint32 Count = 0;
	for( uint32 i = 0; i < BufLen; ++i )
	{
		if( Peek( ValBuf[i] ) )
		{
			++Count;
		}
		else
		{
			break;
		}
	}
	return Count;
}

template< typename DataType, uint32 BufferDataSize >
int32 TRingBuffer< DataType, BufferDataSize >::SerialCompare( const DataType* SerialBuffer, uint32 CompareLen ) const
{
	check( CompareLen <= BufferDataSize);
	const uint32 FirstPartLen = BufferDataSize - DataIndex;
	int32 FirstCmp = FMemory::Memcmp( Data + BottomIndex(), SerialBuffer, sizeof( DataType ) * FMath::Min( FirstPartLen, CompareLen ) );
	if( FirstCmp != 0 || FirstPartLen >= CompareLen)
	{
		return FirstCmp;
	}
	return FMemory::Memcmp( Data, &SerialBuffer[ FirstPartLen ], sizeof( DataType ) * ( CompareLen - FirstPartLen ) );
}

template< typename DataType, uint32 BufferDataSize >
void TRingBuffer< DataType, BufferDataSize >::Serialize( DataType* SerialBuffer ) const
{
	const uint32 FirstPartLen = BufferDataSize - DataIndex;
 	FMemory::Memcpy( SerialBuffer, Data + BottomIndex(), sizeof( DataType ) * FirstPartLen );
 	FMemory::Memcpy( &SerialBuffer[ FirstPartLen ], Data, sizeof( DataType ) * ( BufferDataSize - FirstPartLen ) );
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE const DataType& TRingBuffer< DataType, BufferDataSize >::operator[]( const int32& Index ) const
{
	return Data[ ( BottomIndex() + Index ) % BufferDataSize ];
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE DataType& TRingBuffer< DataType, BufferDataSize >::operator[]( const int32& Index )
{
	return Data[ ( BottomIndex() + Index ) % BufferDataSize ];
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE const DataType& TRingBuffer< DataType, BufferDataSize >::Top() const
{
	return Data[ TopIndex() ];
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE DataType& TRingBuffer< DataType, BufferDataSize >::Top()
{
	return Data[ TopIndex() ];
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE const DataType& TRingBuffer< DataType, BufferDataSize >::Bottom() const
{
	return Data[ BottomIndex() ];
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE DataType& TRingBuffer< DataType, BufferDataSize >::Bottom()
{
	return Data[ BottomIndex() ];
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE const uint32 TRingBuffer< DataType, BufferDataSize >::TopIndex() const
{
	return ( DataIndex + BufferDataSize - 1 ) % BufferDataSize;
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE const uint32 TRingBuffer< DataType, BufferDataSize >::BottomIndex() const
{
	return ( DataIndex + BufferDataSize - NumDataAvailable ) % BufferDataSize;
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE const uint32 TRingBuffer< DataType, BufferDataSize >::NextIndex() const
{
	return DataIndex;
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE const uint32 TRingBuffer< DataType, BufferDataSize >::RingDataSize() const
{
	return BufferDataSize;
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE const uint32 TRingBuffer< DataType, BufferDataSize >::RingDataUsage() const
{
	return NumDataAvailable;
}

template< typename DataType, uint32 BufferDataSize >
FORCEINLINE const uint64 TRingBuffer< DataType, BufferDataSize >::TotalDataPushed() const
{
	return TotalNumDataPushed;
}

/* FRollingHash implementation
*****************************************************************************/
template< uint32 WindowSize >
FRollingHash< WindowSize >::FRollingHash()
	: HashState( 0 )
	, NumBytesConsumed( 0 )
{
}

template< uint32 WindowSize >
void FRollingHash< WindowSize >::ConsumeByte( const uint8& NewByte )
{
	// We must be setup by consuming the correct amount of bytes
	check( NumBytesConsumed < WindowSize );
	++NumBytesConsumed;

	// Add the byte to our buffer
	WindowData.Enqueue( NewByte );
	// Add to our HashState
	ROTLEFT_64B( HashState, 1 );
	HashState ^= FRollingHashConst::HashTable[ NewByte ];
}

template< uint32 WindowSize >
void FRollingHash< WindowSize >::ConsumeBytes( const uint8* NewBytes, const uint32& NumBytes )
{
	for ( uint32 i = 0; i < NumBytes; ++i )
	{
		ConsumeByte( NewBytes[i] );
	}
}

template< uint32 WindowSize >
const uint32 FRollingHash< WindowSize >::GetNumDataNeeded() const
{
	return WindowSize - NumBytesConsumed;
}

template< uint32 WindowSize >
const uint32 FRollingHash< WindowSize >::GetWindowSize() const
{
	return WindowSize;
}

template< uint32 WindowSize >
void FRollingHash< WindowSize >::RollForward( const uint8& NewByte )
{
	// We must have consumed enough bytes to function correctly
	check( NumBytesConsumed == WindowSize );
	uint8 OldByte;
	WindowData.Dequeue( OldByte );
	WindowData.Enqueue( NewByte );
	// Update our HashState
	uint64 OldTerm = FRollingHashConst::HashTable[ OldByte ];
	ROTLEFT_64B( OldTerm, WindowSize );
	ROTLEFT_64B( HashState, 1 );
	HashState ^= OldTerm;
	HashState ^= FRollingHashConst::HashTable[ NewByte ];
}

template< uint32 WindowSize >
void FRollingHash< WindowSize >::Clear()
{
	HashState = 0;
	NumBytesConsumed = 0;
	WindowData.Empty();
}

template< uint32 WindowSize >
const uint64 FRollingHash< WindowSize >::GetWindowHash() const
{
	// We must have consumed enough bytes to function correctly
	check( NumBytesConsumed == WindowSize );
	return HashState;
}

template< uint32 WindowSize >
const TRingBuffer< uint8, WindowSize >& FRollingHash< WindowSize >::GetWindowData() const
{
	return WindowData;
}

template< uint32 WindowSize >
uint64 FRollingHash< WindowSize >::GetHashForDataSet( const uint8* DataSet )
{
	uint64 HashState = 0;
	for ( uint32 i = 0; i < WindowSize; ++i )
	{
		ROTLEFT_64B( HashState, 1 );
		HashState ^= FRollingHashConst::HashTable[ DataSet[i] ];
	}
	return HashState;
}

/**
* A static function to perform sanity checks on the rolling hash class.
*/
static bool CheckRollingHashAlgorithm()
{
	bool bCheckOk = true;
	// Sanity Check the RollingHash code!!
	FString IndivWords[6];
	IndivWords[0] = "123456";	IndivWords[1] = "7890-=";	IndivWords[2] = "qwerty";	IndivWords[3] = "uiop[]";	IndivWords[4] = "asdfgh";	IndivWords[5] = "jkl;'#";
	FString DataToRollOver = FString::Printf( TEXT( "%s%s%s%s%s%s" ), *IndivWords[0], *IndivWords[1], *IndivWords[2], *IndivWords[3], *IndivWords[4], *IndivWords[5] );
	uint64 IndivHashes[6];
	for (uint32 idx = 0; idx < 6; ++idx )
	{
		uint8 Converted[6];
		for (uint32 iChar = 0; iChar < 6; ++iChar )
			Converted[iChar] = IndivWords[idx][iChar];
		IndivHashes[idx] = FRollingHash< 6 >::GetHashForDataSet( Converted );
	}

	FRollingHash< 6 > RollingHash;
	uint32 StrIdx = 0;
	for (uint32 k=0; k<6; ++k)
		RollingHash.ConsumeByte( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[0] == RollingHash.GetWindowHash();
	for (uint32 k=0; k<6; ++k)
		RollingHash.RollForward( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[1] == RollingHash.GetWindowHash();
	for (uint32 k=0; k<6; ++k)
		RollingHash.RollForward( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[2] == RollingHash.GetWindowHash();
	for (uint32 k=0; k<6; ++k)
		RollingHash.RollForward( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[3] == RollingHash.GetWindowHash();
	for (uint32 k=0; k<6; ++k)
		RollingHash.RollForward( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[4] == RollingHash.GetWindowHash();
	for (uint32 k=0; k<6; ++k)
		RollingHash.RollForward( DataToRollOver[ StrIdx++ ] );
	bCheckOk = bCheckOk && IndivHashes[5] == RollingHash.GetWindowHash();

	return bCheckOk;
}

#endif // __BuildPatchHash_h__
