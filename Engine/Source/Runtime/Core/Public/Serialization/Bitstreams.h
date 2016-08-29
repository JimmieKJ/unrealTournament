// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


CORE_API void appBitsCpy( uint8* Dest, int32 DestBit, uint8* Src, int32 SrcBit, int32 BitCount );


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

	/**
	 * Serializes a compressed integer - Value must be < Max
	 *
	 * @param Value		The value to serialize, must be < Max
	 * @param Max		The maximum allowed value - good to aim for power-of-two
	 */
	void SerializeInt(uint32& Value, uint32 Max);

	/**
	 * Serializes the specified Value, but does not bounds check against ValueMax;
	 * instead, it will wrap around if the value exceeds ValueMax (this differs from SerializeInt, which clamps)
	 *
	 * @param Value		The value to serialize
	 * @param ValueMax	The maximum value to write; wraps Value if it exceeds this
	 */
	void WriteIntWrapped(uint32 Value, uint32 ValueMax);

	void WriteBit( uint8 In );
	void Serialize( void* Src, int64 LengthBytes );

	/**
	 * Returns a pointer to our internal buffer.
	 */
	FORCEINLINE uint8* GetData(void)
	{
#if !UE_BUILD_SHIPPING
		// If this happens, your code has insufficient IsError() checks.
		check(!IsError());
#endif

		return Buffer.GetData();
	}

	FORCEINLINE const uint8* GetData(void) const
	{
#if !UE_BUILD_SHIPPING
		// If this happens, your code has insufficient IsError() checks.
		check(!IsError());
#endif

		return Buffer.GetData();
	}

	FORCEINLINE const TArray<uint8>* GetBuffer(void) const
	{
#if !UE_BUILD_SHIPPING
		// If this happens, your code has insufficient IsError() checks.
		check(!IsError());
#endif

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
	 *
	 * @param LengthBits	The number of bits being written at the time of overflow
	 */
	void SetOverflowed(int32 LengthBits);

	/**
	 * Sets whether or not this writer intentionally allows overflows (to avoid logspam)
	 */
	FORCEINLINE void SetAllowOverflow(bool bInAllow)
	{
		bAllowOverflow = bInAllow;
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

	/** Whether or not overflowing is allowed (overflows silently) */
	bool bAllowOverflow;
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
	FORCEINLINE_DEBUGGABLE void SerializeBits( void* Dest, int64 LengthBits )
	{
		if ( IsError() || Pos+LengthBits > Num)
		{
			if (!IsError())
			{
				SetOverflowed(LengthBits);
				//UE_LOG( LogNetSerialization, Error, TEXT( "FBitReader::SerializeBits: Pos + LengthBits > Num" ) );
			}
			FMemory::Memzero( Dest, (LengthBits+7)>>3 );
			return;
		}
		//for( int32 i=0; i<LengthBits; i++,Pos++ )
		//	if( Buffer(Pos>>3) & GShift[Pos&7] )
		//		((uint8*)Dest)[i>>3] |= GShift[i&7];
		if( LengthBits == 1 )
		{
			((uint8*)Dest)[0] = 0;
			if( Buffer[Pos>>3] & Shift(Pos&7) )
				((uint8*)Dest)[0] |= 0x01;
			Pos++;
		}
		else if (LengthBits != 0)
		{
			((uint8*)Dest)[((LengthBits+7)>>3) - 1] = 0;
			appBitsCpy((uint8*)Dest, 0, Buffer.GetData(), Pos, LengthBits);
			Pos += LengthBits;
		}
	}

	// OutValue < ValueMax
	FORCEINLINE_DEBUGGABLE void SerializeInt(uint32& OutValue, uint32 ValueMax)
	{
		if (!IsError())
		{
			// Use local variable to avoid Load-Hit-Store
			uint32 Value = 0;
			int64 LocalPos = Pos;
			const int64 LocalNum = Num;

			for (uint32 Mask=1; (Value + Mask) < ValueMax && Mask; Mask *= 2, LocalPos++)
			{
				if (LocalPos >= LocalNum)
				{
					SetOverflowed(LocalPos - Pos);
					break;
				}

				if (Buffer[LocalPos >> 3] & Shift(LocalPos & 7))
				{
					Value |= Mask;
				}
			}

			// Now write back
			Pos = LocalPos;
			OutValue = Value;
		}
	}

	FORCEINLINE_DEBUGGABLE uint32 ReadInt(uint32 Max)
	{
		uint32 Value = 0;

		SerializeInt(Value, Max);

		return Value;
	}

	FORCEINLINE_DEBUGGABLE uint8 ReadBit()
	{
		uint8 Bit=0;
		//SerializeBits( &Bit, 1 );
		if ( !IsError() )
		{
			int64 LocalPos = Pos;
			const int64 LocalNum = Num;
			if (LocalPos >= LocalNum)
			{
				SetOverflowed(1);
				//UE_LOG( LogNetSerialization, Error, TEXT( "FBitReader::SerializeInt: LocalPos >= LocalNum" ) );
			}
			else
			{
				Bit = !!(Buffer[LocalPos>>3] & Shift(LocalPos&7));
				Pos++;
			}
		}
		return Bit;
	}

	FORCEINLINE_DEBUGGABLE void Serialize( void* Dest, int64 LengthBytes )
	{
		SerializeBits( Dest, LengthBytes*8 );
	}

	FORCEINLINE_DEBUGGABLE uint8* GetData()
	{
		return Buffer.GetData();
	}

	FORCEINLINE_DEBUGGABLE uint8* GetDataPosChecked()
	{
		check(Pos % 8 == 0);
		return &Buffer[Pos >> 3];
	}

	FORCEINLINE_DEBUGGABLE uint32 GetBytesLeft()
	{
		return ((Num - Pos) + 7) >> 3;
	}
	FORCEINLINE_DEBUGGABLE uint32 GetBitsLeft()
	{
		return (Num - Pos);
	}
	FORCEINLINE_DEBUGGABLE bool AtEnd()
	{
		return ArIsError || Pos>=Num;
	}
	FORCEINLINE_DEBUGGABLE int64 GetNumBytes()
	{
		return (Num+7)>>3;
	}
	FORCEINLINE_DEBUGGABLE int64 GetNumBits()
	{
		return Num;
	}
	FORCEINLINE_DEBUGGABLE int64 GetPosBits()
	{
		return Pos;
	}
	FORCEINLINE_DEBUGGABLE void EatByteAlign()
	{
		int32 PrePos = Pos;

		// Skip over remaining bits in current byte
		Pos = (Pos+7) & (~0x07);

		if ( Pos > Num )
		{
			//UE_LOG( LogNetSerialization, Error, TEXT( "FBitReader::EatByteAlign: Pos > Num" ) );
			SetOverflowed(Pos - PrePos);
		}
	}

	/**
	 * Marks this bit reader as overflowed.
	 *
	 * @param LengthBits	The number of bits being read at the time of overflow
	 */
	void SetOverflowed(int32 LengthBits);

	void AppendDataFromChecked( FBitReader& Src );
	void AppendDataFromChecked( uint8* Src, uint32 NumBits );
	void AppendTo( TArray<uint8> &Buffer );

protected:

	TArray<uint8> Buffer;
	int64 Num;
	int64 Pos;

private:

	FORCEINLINE uint8 Shift(uint8 Cnt)
	{
		return 1<<Cnt;
	}

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
