// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/*-----------------------------------------------------------------------------
	FBitWriter.
-----------------------------------------------------------------------------*/

//
// Writes bitstreams.
//
struct CORE_API FBitWriter : public FArchive
{
	friend struct FBitWriterMark;

public:
	/** Default constructor. Zeros everything. */
	FBitWriter(void);

	/**
	 * Constructor using known size the buffer needs to be
	 */
	FBitWriter( int64 InMaxBits, bool AllowResize = false );

	void SerializeBits( void* Src, int64 LengthBits );
	void SerializeInt( uint32& Value, uint32 Max );

	/** serializes the specified value, but does not bounds check against Max; instead, it will wrap around if the value exceeds Max
	 * (this differs from SerializeInt(), which clamps)
	 * @param Result - the value to serialize
	 * @param Max - maximum value to write; wrap Result if it exceeds this
	 */
	void WriteIntWrapped( uint32 Result, uint32 Max );
	void WriteBit( uint8 In );
	void Serialize( void* Src, int64 LengthBytes );

	/**
	 * Returns a pointer to our internal buffer.
	 */
	FORCEINLINE uint8* GetData(void)
	{
		return Buffer.GetData();
	}

	FORCEINLINE const uint8* GetData(void) const
	{
		return Buffer.GetData();
	}

	FORCEINLINE const TArray<uint8>* GetBuffer(void) const
	{
		return &Buffer;
	}

	/**
	 * Returns the number of bytes written.
	 */
	FORCEINLINE int64 GetNumBytes(void) const
	{
		return (Num+7)>>3;
	}

	/**
	 * Returns the number of bits written.
	 */
	FORCEINLINE int64 GetNumBits(void) const
	{
		return Num;
	}

	/**
	 * Returns the number of bits the buffer supports.
	 */
	FORCEINLINE int64 GetMaxBits(void) const
	{
		return Max;
	}

	/**
	 * Marks this bit writer as overflowed.
	 */
	FORCEINLINE void SetOverflowed(void)
	{
		ArIsError = 1;
	}

	FORCEINLINE bool AllowAppend(int64 LengthBits)
	{
		if (Num+LengthBits > Max)
		{
			if (AllowResize)
			{
				// Resize our buffer. The common case for resizing bitwriters is hitting the max and continuing to add a lot of small segments of data
				// Though we could just allow the TArray buffer to handle the slack and resizing, we would still constantly hit the FBitWriter's max
				// and cause this block to be executed, as well as constantly zeroing out memory inside AddZeroes (though the memory would be allocated
				// in chunks).

				Max = FMath::Max<int64>(Max<<1,Num+LengthBits);
				int32 ByteMax = (Max+7)>>3;
				Buffer.AddZeroed(ByteMax - Buffer.Num());
				check((Max+7)>>3== Buffer.Num());
				return true;
			}
			else
			{
				return false;
			}
		}
		return true;
	}

	FORCEINLINE void SetAllowResize(bool NewResize)
	{
		AllowResize = NewResize;
	}

	/**
	 * Resets the bit writer back to its initial state
	 */
	void Reset(void);

	FORCEINLINE void WriteAlign()
	{
		Num = ( Num + 7 ) & ( ~0x07 );
	}

private:

	TArray<uint8> Buffer;
	int64   Num;
	int64   Max;
	bool	AllowResize;
};


//
// For pushing and popping FBitWriter positions.
//
struct CORE_API FBitWriterMark
{
public:

	FBitWriterMark()
		: Overflowed(false)
		, Num(0)
	{ }

	FBitWriterMark( FBitWriter& Writer )
	{
		Init(Writer);
	}

	int64 GetNumBits()
	{
		return Num;
	}

	void Init( FBitWriter& Writer)
	{
		Num = Writer.Num;
		Overflowed = Writer.ArIsError;
	}

	void Pop( FBitWriter& Writer );
	void Copy( FBitWriter& Writer, TArray<uint8> &Buffer );
	void PopWithoutClear( FBitWriter& Writer );

private:

	bool			Overflowed;
	int64			Num;
};


/*-----------------------------------------------------------------------------
	FBitReader.
-----------------------------------------------------------------------------*/

//
// Reads bitstreams.
//
struct CORE_API FBitReader : public FArchive
{
	friend struct FBitReaderMark;

public:

	FBitReader( uint8* Src = nullptr, int64 CountBits = 0 );
	void SetData( FBitReader& Src, int64 CountBits );
	void SerializeBits( void* Dest, int64 LengthBits );
	void SerializeInt( uint32& Value, uint32 Max );
	uint32 ReadInt( uint32 Max );
	uint8 ReadBit();
	void Serialize( void* Dest, int64 LengthBytes );
	uint8* GetData();
	uint8* GetDataPosChecked();
	uint32 GetBytesLeft();
	uint32 GetBitsLeft();
	bool AtEnd();
	void SetOverflowed();
	int64 GetNumBytes();
	int64 GetNumBits();
	int64 GetPosBits();
	void AppendDataFromChecked( FBitReader& Src );
	void AppendDataFromChecked( uint8* Src, uint32 NumBits );
	void AppendTo( TArray<uint8> &Buffer );
	void EatByteAlign();

protected:

	TArray<uint8> Buffer;
	int64 Num;
	int64 Pos;
};


//
// For pushing and popping FBitWriter positions.
//
struct CORE_API FBitReaderMark
{
public:

	FBitReaderMark()
		: Pos(0)
	{ }

	FBitReaderMark( FBitReader& Reader )
		: Pos(Reader.Pos)
	{ }

	int64 GetPos()
	{
		return Pos;
	}

	void Pop( FBitReader& Reader )
	{
		Reader.Pos = Pos;
	}

	void Copy( FBitReader& Reader, TArray<uint8> &Buffer );

private:

	int64 Pos;
};
